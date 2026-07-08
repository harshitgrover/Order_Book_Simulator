#pragma once

#include "engine/order.hpp"
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <iostream>

using namespace std;

struct PriceLevel {
    double price;
    list<Order> orders;
};

class OrderBook {
private:
    map<double, PriceLevel, greater<double>> bids;
    map<double, PriceLevel> asks;

    struct OrderIndexEntry {
        Side side;
        double price;
        list<Order>::iterator iterator;
    };
    unordered_map<uint64_t, OrderIndexEntry> order_index;

public:
    vector<Trade> addLimitOrder(Order order);
    bool cancelOrder(uint64_t order_id);
    vector<Trade> addMarketOrder(Order order);
    vector<Trade> addIocOrder(Order order);
    
    bool getOrder(uint64_t order_id, Order& out_order) const;
    
    double bestBid() const;
    double bestAsk() const;
    double midPrice() const;

    void printBook() const;
    string toJson() const;
};
