#pragma once

#include <stdexcept>
#include <string>

namespace ftx
{
namespace ws
{

enum class Side
    : int
{
    BUY = 0,
    SELL = 1,
    NUM_SIDES = 2
};

static Side SideFromString(const std::string& side_str)
{
    if (side_str == "buy")
    {
        return Side::BUY;
    }
    else if (side_str == "sell")
    {
        return Side::SELL;
    }
    else
    {
        throw std::runtime_error("Invalid side string");
        return Side::NUM_SIDES;
    }
}

static std::string SideToString(const Side side)
{
    switch(side)
    {
        case Side::BUY:
            return "buy";
        case Side::SELL:
            return "sell";
        default:
            return "";
    }
}

struct Bbo
{
    struct BidAsk
    {
        double bid;
        double ask;
    };

    BidAsk price;
    BidAsk size;
};

struct Fill
{
    double fee;
    double fee_rate;
    std::string market;
    int64_t order_id;
    int64_t trade_id;
    double price;
    double size;
    Side side;
};

struct Order
{
    enum class Status
    {
        NEW,
        OPEN,
        CLOSED,
        NONE
    };

    static Status StatusFromString(const std::string& status)
    {
        if (status == "new")
        {
            return Status::NEW;
        }
        else if (status == "open")
        {
            return Status::OPEN;
        }
        else if (status == "closed")
        {
            return Status::CLOSED;
        }
        else
        {
            throw std::runtime_error("Invalid status");
            return Status::NONE;
        }
    }

    int64_t order_id;
    std::string client_id;
    std::string market;
    Side side;
    
    double price;
    double size;
    double filled_size;
    double remaining_size;

    Status status;
};

} // namespace ws
} // namespace ftx