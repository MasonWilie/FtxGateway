## Description
This project is obviously not very useful anymore, but I thought it'd be interesting to put it out there anyways. The goal of this app is to just try to get around market taking fees by continuously joining the bid or ask, hoping to get taking. It works, but is slow (not the app itself, the strategy) and can potentially result in a worse price anyway considering that it exposes you to movement in the market (though if you believe that the price is a random walk then it's equally likely that you'd get a better price, not considering momentum which shouldn't play much of a factor considering that the exposure is still only seconds). If you are on an exchange that allows you to actually collect maker fees (some do if you hold a certain amount of their coin) or there are no maker fees, then this may actually be worth it.

## Dependencies

[Boost](https://www.boost.org/)

[OpenSSL](https://www.openssl.org/)

This project also depends on [RapidJson](https://github.com/Tencent/rapidjson) and [CPR](https://github.com/libcpr/cpr), but they _should_ automatically be available.

## Platforms

Tested with GCC 9.4.0 on Ubuntu 20.04

## Building

Starting in the root directory, run the following

```bash
$ mkdir build
$ cd build
$ cmake ..
$ make
```
This should generate an executable called FtxReduceMtFee

## Running

To start the program, execute the following
```bash
$ ./FtxReduceMtFee "<api_key>" "<api_secret>" "<market>"
```

This should bring up a basic console. From there, enter a command.

```
s -- Send a sell order
b -- Send a buy order
q -- Quit
```

The next prompt will ask for a size (units are in whatever coin you are buying or selling)

This should send an order, and eventually the order will fill and information about the fill will be posted.

```
--- Fill ---
Original order price: 1181.4, Original market price: 1181.2, Fill price: 1181.2, slippage: 0, Times queued: 14
```

Here is a description of these values
```
Original order price -- Price of the first limit order placed
Original market price -- Price that the market order would have filled at
Fill price -- Price that you did fill at
Slippage -- Percentage of price improvement using this strategy (not including fees)
Times queued -- Number of orders/cancels placed to get this fill
```

## Strategy
This application implements a pretty naive strategy of just repeatedly improving the BBO by one tick until the entire order is filled.

## Issues
* Executions are much slower than regular market orders, since this strategy requires the market price to move into your order
* The final fill price can be worse than what it would have been if you were to just place a market order. But the fee reduction helps negate this.