#pragma once

#include "FtxAPI.h"
#include "FtxWebSocket.h"

#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace ftx
{

class Gateway
{
public:
    explicit Gateway(const std::string& key, const std::string& secret, const std::string& market);
    virtual ~Gateway();

    void SendMarketOrder(const ws::Side side, const double size);

private:

    struct OutstandingOrder
    {
        enum class State
            : int
        {
            SENT = 0,
            QUEUED = 1,
            RESTING = 2,
            PENDING_CANCEL = 3
        };

        State state;

        uint64_t client_id;
        ws::Side side;

        uint64_t original_time_ns;
        double original_market_price;
        double original_order_price;
        double original_size;
        double filled_size;

        uint64_t queued_count;
    };

    void SendMarketOrder(const ws::Side side, const double size, const uint64_t client_id, const bool new_order = true);

    void SetInitialMarketData();
    void SetWebsocketCallbacks();

    void OnBboUpdate(const ws::Bbo& bbo);
    void OnOrderUpdate(const ws::Order& order);

    void HandleNewOrder(const std::shared_ptr<OutstandingOrder>& outstanding_order, const ws::Order& order);
    void HandleOpenOrder(const std::shared_ptr<OutstandingOrder>& outstanding_order, const ws::Order& order);
    void HandleClosedOrder(const std::shared_ptr<OutstandingOrder>& outstanding_order, const ws::Order& order);

    void Disable(const char* error);
    void CancelAll();

    const FtxAPI _api;
    ws::FtxWebSocket _web_socket;
    const std::string _market;
    
    std::atomic<uint64_t> _next_order_id;
    std::atomic<bool> _running;

    double _tick_price;

    std::mutex _bbo_mtx;
    ws::Bbo _current_bbo;

    using OrderMap_t = std::unordered_map<uint64_t, std::shared_ptr<OutstandingOrder>>;

    std::mutex _orders_mtx;
    OrderMap_t _orders;
};

} // namespace ftx