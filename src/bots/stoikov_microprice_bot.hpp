#pragma once

#include "engine/order_book.hpp"
#include <memory>
#include <cmath>
#include <iostream>

using namespace std;

class StoikovMicropriceBot {
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
    double last_micro_price;

public:
    StoikovMicropriceBot(shared_ptr<OrderBook> book, double gamma, double sigma, double k, double T);

    void updateInventory();
    void onTick(double dt);
    void cancelAll();
    
    int getInventory() const { return inventory; }
    double getCash() const { return cash; }
    double getMicroPrice() const { 
        double best_bid = order_book->bestBid();
        double best_ask = order_book->bestAsk();
        if (best_bid == 0.0 || best_ask == 0.0) return order_book->midPrice();
        uint64_t bid_vol = order_book->bestBidVolume();
        uint64_t ask_vol = order_book->bestAskVolume();
        if (bid_vol + ask_vol > 0) {
            return (best_bid * ask_vol + best_ask * bid_vol) / static_cast<double>(bid_vol + ask_vol);
        }
        return order_book->midPrice();
    }
    double getPnl(double mid_price) const {
        return cash + (inventory * mid_price);
    }
    double getActiveBidPrice() const { return active_bid_id != 0 ? active_bid_price : 0.0; }
    double getActiveAskPrice() const { return active_ask_id != 0 ? active_ask_price : 0.0; }
    uint64_t getActiveBidQty() const { return active_bid_id != 0 ? active_bid_qty : 0; }
    uint64_t getActiveAskQty() const { return active_ask_id != 0 ? active_ask_qty : 0; }
};
