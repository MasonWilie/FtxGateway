#include "Gateway.h"

#include <chrono>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <thread>

namespace ftx
{

namespace
{

static inline bool Equal(const double a, const double b, const double epsilon = 1e-9)
{
    return std::fabs(a - b) < epsilon;
}

static inline double GetSlippagePercentage(const ws::Side side, const double order_price, const double fill_price)
{
    return 100 * (side == ws::Side::BUY
        ? fill_price / order_price - 1
        : order_price / fill_price - 1);
}

}

Gateway::Gateway(const std::string& key, const std::string& secret, const std::string& market)
    : _api(key, secret)
    , _web_socket(market, key, secret)
    , _market(market)
    , _next_order_id(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count())
{
    SetInitialMarketData();
    SetWebsocketCallbacks();
    _running = true;
}

Gateway::~Gateway()
{
    CancelAll();
}

void Gateway::SetInitialMarketData()
{
    const rapidjson::Document response = _api.GetRequest("/markets/" + _market);

    if (!response["success"].GetBool())
    {
        std::cerr << "Failed to retrieve initial market information from AIP" << std::endl;
        throw std::runtime_error("Failed to get initial market data");
    }

    const auto& result = response["result"];

    _tick_price = result["priceIncrement"].GetDouble();

    std::lock_guard<std::mutex> lock(_bbo_mtx);
    _current_bbo.price.bid = result["bid"].GetDouble();
    _current_bbo.price.ask = result["ask"].GetDouble();
    _current_bbo.size.bid = 1;
    _current_bbo.size.ask = 1;
}

void Gateway::SetWebsocketCallbacks()
{
    _web_socket.SetBboCallback([this](const ws::Bbo& bbo){this->OnBboUpdate(bbo);});
    _web_socket.SetOrderCallback([this](const ws::Order& order){this->OnOrderUpdate(order);});
}

void Gateway::SendMarketOrder(const ws::Side side, const double size)
{
    SendMarketOrder(side, size, _next_order_id.fetch_add(1), true);
}

void Gateway::SendMarketOrder(const ws::Side side, const double size, const uint64_t client_id, const bool new_order)
{
    if (!_running)
    {
        return;
    }

    const double order_price = side == ws::Side::BUY ? _current_bbo.price.bid + _tick_price : _current_bbo.price.ask - _tick_price;

    if (new_order)
    {
        auto order_ptr = std::make_shared<OutstandingOrder>();

        {
            std::lock_guard<std::mutex> lock(_orders_mtx);
            _orders.emplace(client_id, order_ptr);
        }

        order_ptr->client_id = client_id;

        order_ptr->original_size = size;
        order_ptr->filled_size = 0.0;

        order_ptr->original_order_price = order_price;
        order_ptr->original_market_price = side == ws::Side::BUY ? _current_bbo.price.ask : _current_bbo.price.bid;

        order_ptr->side = side;

        using namespace std::chrono;
        order_ptr->original_time_ns = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();

        order_ptr->queued_count = 1;

        order_ptr->state = OutstandingOrder::State::SENT;
    }
    
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> body_writer(buffer);

    body_writer.StartObject();
    
    body_writer.Key("market");
    body_writer.String(_market.c_str());

    body_writer.Key("side");
    body_writer.String(ws::SideToString(side).c_str());

    body_writer.Key("price");
    body_writer.Double(order_price);

    body_writer.Key("type");
    body_writer.String("limit");

    body_writer.Key("size");
    body_writer.Double(size);

    body_writer.Key("reduceOnly");
    body_writer.Bool(false);

    body_writer.Key("ioc");
    body_writer.Bool(false);

    body_writer.Key("postOnly");
    body_writer.Bool(true);

    body_writer.Key("clientId");
    body_writer.String(std::to_string(client_id).c_str());

    body_writer.EndObject();

    _api.PostRequest("/orders", buffer.GetString());
}

void Gateway::OnBboUpdate(const ws::Bbo& bbo)
{
    ws::Bbo old_bbo;

    {
        std::lock_guard<std::mutex> lock(_bbo_mtx);
        old_bbo = _current_bbo;
        _current_bbo = bbo;
    }

    std::lock_guard<std::mutex> lock(_orders_mtx);
    for (auto& [client_id, order_ptr] : _orders)
    {
        if (order_ptr->state != OutstandingOrder::State::RESTING
                && order_ptr->state != OutstandingOrder::State::QUEUED)
        {
            continue;
        }

        const auto response = _api.DeleteRequest("/orders/by_client_id/" + std::to_string(client_id));
        order_ptr->state = OutstandingOrder::State::PENDING_CANCEL;
    }
}

void Gateway::OnOrderUpdate(const ws::Order& order)
{
    const uint64_t client_id = std::stoull(order.client_id);

    std::shared_ptr<OutstandingOrder> outstanding_order;

    {
        std::lock_guard<std::mutex> lock(_orders_mtx);
        auto order_iter = _orders.find(client_id);
        if (order_iter == std::end(_orders))
        {
            Disable("Could not find order");
        }
        outstanding_order = order_iter->second;
    }

    switch (order.status)
    {
    case ws::Order::Status::NEW:
        {
            HandleNewOrder(outstanding_order, order);
        }
        break;
    case ws::Order::Status::OPEN:
        {
            HandleOpenOrder(outstanding_order, order);
        }
        break;
    case ws::Order::Status::CLOSED:
        {
            HandleClosedOrder(outstanding_order, order);
        }
        break;
    
    default:
        break;
    }
}

void Gateway::HandleNewOrder(const std::shared_ptr<OutstandingOrder>& outstanding_order, const ws::Order& order)
{
    std::lock_guard<std::mutex> lock(_orders_mtx);
    if (outstanding_order->state != OutstandingOrder::State::SENT)
    {
        Disable("Got a 'NEW' order with a status other than 'SENT'");
    }

    outstanding_order->state = OutstandingOrder::State::QUEUED;
}

void Gateway::HandleOpenOrder(const std::shared_ptr<OutstandingOrder>& outstanding_order, const ws::Order& order)
{
    std::lock_guard<std::mutex> lock(_orders_mtx);
    if (outstanding_order->state != OutstandingOrder::State::QUEUED)
    {
        std::cerr << "Wrong state: " << static_cast<int>(outstanding_order->state) << std::endl;
        Disable("Got a 'OPEN' order with a status other than 'QUEUED'");
    }

    outstanding_order->state = OutstandingOrder::State::RESTING;
}

void Gateway::HandleClosedOrder(const std::shared_ptr<OutstandingOrder>& outstanding_order, const ws::Order& order)
{
    double size_left = 0.0;

    {
        std::lock_guard<std::mutex> lock(_orders_mtx);

        if (Equal(order.filled_size, order.size))
        {
            std::cout << "--- Fill ---\n"
                << "Original order price: " << outstanding_order->original_order_price
                << ", Original market price: " << outstanding_order->original_market_price
                << ", Fill price: " << order.price
                << ", slippage: " << GetSlippagePercentage(order.side, outstanding_order->original_market_price, order.price)
                << ", Times queued: " << outstanding_order->queued_count << std::endl;
            _orders.erase(outstanding_order->client_id);
            return;
        }

        outstanding_order->filled_size += order.filled_size;
        size_left = outstanding_order->original_size - outstanding_order->filled_size;
        outstanding_order->state = OutstandingOrder::State::SENT;

        _orders.erase(outstanding_order->client_id);
        outstanding_order->client_id = _next_order_id.fetch_add(1);
        _orders.emplace(outstanding_order->client_id, outstanding_order);
    }

    outstanding_order->queued_count++;
    SendMarketOrder(outstanding_order->side, size_left, outstanding_order->client_id, false);
}

void Gateway::Disable(const char* error)
{
    _running = false;

    CancelAll();

    std::cerr << "Trading disabled: " << error << std::endl;
    throw std::runtime_error(error);
}

void Gateway::CancelAll()
{
    const auto response = _api.DeleteRequest("/orders");

    if (!response["success"].GetBool())
    {
        std::cerr << "Unable to cancel all orders!" << std::endl;
    }
}

} // namespace ftx