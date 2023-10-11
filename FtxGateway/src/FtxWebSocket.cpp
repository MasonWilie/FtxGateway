#include "FtxWebSocket.h"

#include <chrono>
#include <ctime>

#include <boost/bind.hpp>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <websocketpp/endpoint.hpp>

#include "HmacSha256.hpp"

namespace ftx
{
namespace ws
{

FtxWebSocket::FtxWebSocket(const std::string& market
        , const std::string& key
        , const std::string& secret
        , const std::string& endpoint)
    : _market(market)
    , _key(key)
    , _secret(secret)
    , _client()
    , _bbo_callback([](const Bbo&){})
    , _order_callback([](const Order&){})
    , _fill_callback([](const Fill&){})
    , _running(false)
{
    _client.clear_access_channels(websocketpp::log::alevel::all);
    _client.clear_error_channels(websocketpp::log::elevel::all);
    _client.init_asio();
    
    _client.set_open_handler(boost::bind(&FtxWebSocket::OnOpen, this, &_client, boost::placeholders::_1));
    _client.set_fail_handler(boost::bind(&FtxWebSocket::OnFail, this, &_client, boost::placeholders::_1));
    _client.set_message_handler(boost::bind(&FtxWebSocket::OnMessage, this, &_client, boost::placeholders::_1, boost::placeholders::_2));
    _client.set_close_handler(boost::bind(&FtxWebSocket::OnClose, this, &_client, boost::placeholders::_1));
    _client.set_tls_init_handler(boost::bind(&FtxWebSocket::OnTlsInit));

    _client.start_perpetual();

    websocketpp::lib::error_code ec;
    _connection_ptr = _client.get_connection(endpoint, ec);
    _client.connect(_connection_ptr);

    _receiver_thread = std::make_unique<std::thread>([this](){_client.run();});
}

FtxWebSocket::~FtxWebSocket()
{
    Unsubscribe();
    
    _running = false;
    _client.stop();
    if (_receiver_thread)
    {
        _receiver_thread->join();
    }

    if (_heartbeat_thread)
    {
        _heartbeat_thread->join();
    }
}

void FtxWebSocket::SetBboCallback(const BboCallback_t& callback)
{
    _bbo_callback = callback;
}

void FtxWebSocket::SetOrderCallback(const OrderCallback_t& callback)
{
    _order_callback = callback;
}

void FtxWebSocket::SetFillCallback(const FillCallback_t& callback)
{
    _fill_callback = callback;
}

FtxWebSocket::ContextPtr FtxWebSocket::OnTlsInit()
{
    ContextPtr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try
    {
        ctx->set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::no_sslv3
            | boost::asio::ssl::context::single_dh_use
        );

    }
    catch (std::exception &e)
    {
        std::cout << "Error in context pointer: " << e.what() << std::endl;
    }
    
    return ctx;
}

void FtxWebSocket::OnOpen(Client* c, websocketpp::connection_hdl hdl)
{
    _running = true;
    _heartbeat_thread = std::make_unique<std::thread>([this](){this->Heartbeat();});

    Login();
    Subscribe();
}

void FtxWebSocket::Subscribe()
{
    websocketpp::lib::error_code ec;
    _client.send(_connection_ptr->get_handle()
            , "{\"op\":\"subscribe\",\"channel\":\"ticker\",\"market\":\"" + _market + "\"}"
            ,  websocketpp::frame::opcode::text
            , ec);

    _client.send(_connection_ptr->get_handle()
            , "{\"op\": \"subscribe\", \"channel\": \"fills\"}"
            ,  websocketpp::frame::opcode::text
            , ec);

    _client.send(_connection_ptr->get_handle()
            , "{\"op\": \"subscribe\", \"channel\": \"orders\"}"
            ,  websocketpp::frame::opcode::text
            , ec);
}

void FtxWebSocket::Unsubscribe()
{
    websocketpp::lib::error_code ec;
    _client.send(_connection_ptr->get_handle()
            , "{\"op\":\"unsubscribe\",\"channel\":\"ticker\",\"market\":\"" + _market + "\"}"
            ,  websocketpp::frame::opcode::text
            , ec);

    _client.send(_connection_ptr->get_handle()
            , "{\"op\": \"unsubscribe\", \"channel\": \"fills\"}"
            ,  websocketpp::frame::opcode::text
            , ec);

    _client.send(_connection_ptr->get_handle()
            , "{\"op\": \"unsubscribe\", \"channel\": \"orders\"}"
            ,  websocketpp::frame::opcode::text
            , ec);
}

void FtxWebSocket::OnFail(Client* c, websocketpp::connection_hdl hdl)
{
    std::cerr << "Failed to connect to websocket" << std::endl;
    throw std::runtime_error("WS connection failure");
}

void FtxWebSocket::OnMessage(Client* c, websocketpp::connection_hdl hdl, MessagePtr msg)
{
    rapidjson::Document json;

    json.Parse(msg->get_payload().c_str());

    if (!json.HasMember("channel"))
    {
        return;
    }

    const std::string type(json["type"].GetString());

    if (type != "update")
    {
        return;
    }

    const std::string& channel(json["channel"].GetString());

    if (channel == "ticker")
    {
        CreateAndSendBboUpdate(json);
    }
    else if (channel == "orders")
    {
        CreateAndSendOrderUpdate(json);
    }
    else if (channel == "fills")
    {
        CreateAndSendFillUpdate(json);
    }
}

void FtxWebSocket::OnClose(Client* c, websocketpp::connection_hdl hdl)
{
    std::cout << "WS connection closed" << std::endl;
}

void FtxWebSocket::Heartbeat()
{
    static constexpr const int HEARTBEAT_PERIOD_S = 10;
    static constexpr const char* HB_STRING = "{\"op\":\"ping\"}";

    while (_running)
    {
        std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct std::tm* next_hb_time = std::localtime(&now);
        next_hb_time->tm_sec += HEARTBEAT_PERIOD_S;

        websocketpp::lib::error_code ec;
        _client.send(_connection_ptr->get_handle(), HB_STRING, websocketpp::frame::opcode::text, ec);

        std::this_thread::sleep_until(std::chrono::system_clock::from_time_t(mktime(next_hb_time)));
    }
}

void FtxWebSocket::Login()
{
    const int64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> login_json(buffer);

    login_json.StartObject();

    login_json.Key("op");
    login_json.String("login");

    login_json.Key("args");
    login_json.StartObject();

    login_json.Key("key");
    login_json.String(_key.c_str());

    login_json.Key("sign");
    login_json.String(HmacSha256(std::to_string(time_ms) + "websocket_login", _secret).c_str());

    login_json.Key("time");
    login_json.Int64(time_ms);

    login_json.EndObject();
    login_json.EndObject();

    websocketpp::lib::error_code ec;
    _client.send(_connection_ptr->get_handle(), buffer.GetString(),  websocketpp::frame::opcode::text, ec);
}

void FtxWebSocket::CreateAndSendBboUpdate(const rapidjson::Document& json)
{
    ws::Bbo bbo;
    bbo.price.bid = json["data"]["bid"].GetDouble();
    bbo.price.ask = json["data"]["ask"].GetDouble();
    bbo.size.bid = json["data"]["bidSize"].GetDouble();
    bbo.size.ask = json["data"]["askSize"].GetDouble();

    _bbo_callback(bbo);
}

void FtxWebSocket::CreateAndSendOrderUpdate(const rapidjson::Document& json)
{
    Order order;

    const auto& data = json["data"];

    order.order_id = data["id"].GetInt64();

    const auto& client_id = data["clientId"];
    order.client_id = client_id.IsNull() ? "" : client_id.GetString();

    order.market = data["market"].GetString();
    order.side = SideFromString(data["side"].GetString());
    order.price = data["price"].GetDouble();
    order.size = data["size"].GetDouble();
    order.filled_size = data["filledSize"].GetDouble();
    order.remaining_size = data["remainingSize"].GetDouble();
    order.status = Order::StatusFromString(data["status"].GetString());

    _order_callback(order);
}

void FtxWebSocket::CreateAndSendFillUpdate(const rapidjson::Document& json)
{
    // TODO: This is not 100% necessary since this information can be gleaned from orders
}

} // namespace ws
} // namespace ftx