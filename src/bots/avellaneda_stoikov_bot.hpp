#pragma once

#include "engine/order_book.hpp"
#include <memory>
#include <cmath>
#include <iostream>

using namespace std;

class AvellanedaStoikovBot {
private:
    shared_ptr<OrderBook> order_book;
    
    int inventory;
    double cash;
    
    uint64_t next_order_id;
    
    uint64_t active_bid_id;
    uint64_t active_bid_qty;
    double active_bid_price;
    
    uint64_t active_ask_id;
    uint64_t active_ask_qty;
    double active_ask_price;
    
    double risk_aversion;
    double volatility;
    double order_density;
    double terminal_time;
    double current_time;

public:
    AvellanedaStoikovBot(shared_ptr<OrderBook> book, double gamma, double sigma, double k, double T);

    void updateInventory();
    void onTick(double dt);
    void cancelAll();
    
    int getInventory() const { return inventory; }
    double getCash() const { return cash; }
    double getPnl(double mid_price) const {
        return cash + (inventory * mid_price);
    }
    double getActiveBidPrice() const { return active_bid_id != 0 ? active_bid_price : 0.0; }
    double getActiveAskPrice() const { return active_ask_id != 0 ? active_ask_price : 0.0; }
    uint64_t getActiveBidQty() const { return active_bid_id != 0 ? active_bid_qty : 0; }
    uint64_t getActiveAskQty() const { return active_ask_id != 0 ? active_ask_qty : 0; }
};
