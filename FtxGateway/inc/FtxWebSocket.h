#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <rapidjson/document.h>
#include <websocketpp/client.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_client.hpp>

#include "FtxWebSocketMessages.h"

namespace ftx
{
namespace ws
{

class FtxWebSocket
{
private:
    using Client = websocketpp::client<websocketpp::config::asio_tls_client>;
    using MessagePtr = websocketpp::config::asio_client::message_type::ptr;
    using ContextPtr = std::shared_ptr<boost::asio::ssl::context>;

public:
    using BboCallback_t = std::function<void(const Bbo& bbo)>;
    using OrderCallback_t = std::function<void(const Order& order)>;
    using FillCallback_t = std::function<void(const Fill& fill)>;

    explicit FtxWebSocket(const std::string& market
            , const std::string& key
            , const std::string& secret
            , const std::string& endpoint = "wss://ftx.us/ws/");
    virtual ~FtxWebSocket();

    void SetBboCallback(const BboCallback_t& callback);
    void SetOrderCallback(const OrderCallback_t& callback);
    void SetFillCallback(const FillCallback_t& callback);

private:

    static ContextPtr OnTlsInit();
    void OnOpen(Client* c, websocketpp::connection_hdl hdl);
    void OnFail(Client* c, websocketpp::connection_hdl hdl);
    void OnMessage(Client* c, websocketpp::connection_hdl hdl, MessagePtr msg);
    void OnClose(Client* c, websocketpp::connection_hdl hdl);

    void Heartbeat();
    void Login();
    void Subscribe();
    void Unsubscribe();

    void CreateAndSendBboUpdate(const rapidjson::Document& json);
    void CreateAndSendOrderUpdate(const rapidjson::Document& json);
    void CreateAndSendFillUpdate(const rapidjson::Document& json);

    const std::string _market;
    const std::string _key;
    const std::string _secret;

    Client _client;
    Client::connection_ptr _connection_ptr;

    BboCallback_t _bbo_callback;
    OrderCallback_t _order_callback;
    FillCallback_t _fill_callback;
    
    std::unique_ptr<std::thread> _receiver_thread;
    std::unique_ptr<std::thread> _heartbeat_thread;

    std::atomic<bool> _running;
};

} // namespace ws
} // namespace ftx