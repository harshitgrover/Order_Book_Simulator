#pragma once

#include "engine/order.hpp"
#include <unordered_map>
#include <vector>
#include <iostream>
#include <cmath>

using namespace std;

struct OrderNode {
    Order order;
    uint32_t next = 0;
    uint32_t prev = 0;
};

struct PriceLevel {
    double price = 0.0;
    uint32_t head = 0; // 0 represents nullptr
    uint32_t tail = 0;
};

class OrderBook {
private:
    static constexpr size_t MAX_PRICE_TICKS = 100000; // up to $1000.00
    vector<PriceLevel> bids;
    vector<PriceLevel> asks;

    size_t best_bid_idx;
    size_t best_ask_idx;

    // Object Pool for zero-allocation
    vector<OrderNode> node_pool;
    uint32_t first_free = 0;
    
    uint32_t allocateNode();
    void freeNode(uint32_t node_idx);

    struct OrderIndexEntry {
        Side side;
        size_t price_idx;
        uint32_t node_idx;
    };
    unordered_map<uint64_t, OrderIndexEntry> order_index;

    size_t priceToIndex(double price) const {
        return static_cast<size_t>(std::round(price * 100.0));
    }

    void updateBestBidIdxDown() {
        while (best_bid_idx > 0 && bids[best_bid_idx].head == 0) {
            best_bid_idx--;
        }
        if (best_bid_idx == 0 && bids[0].head == 0) {
            best_bid_idx = MAX_PRICE_TICKS;
        }
    }

    void updateBestAskIdxUp() {
        while (best_ask_idx < MAX_PRICE_TICKS - 1 && asks[best_ask_idx].head == 0) {
            best_ask_idx++;
        }
        if (best_ask_idx == MAX_PRICE_TICKS - 1 && asks[best_ask_idx].head == 0) {
            best_ask_idx = MAX_PRICE_TICKS;
        }
    }

public:
    OrderBook();

    vector<Trade> addLimitOrder(Order order);
    bool cancelOrder(uint64_t order_id);
    bool modifyOrder(uint64_t order_id, uint64_t new_qty);
    vector<Trade> addMarketOrder(Order order);
    vector<Trade> addIocOrder(Order order);
    
    bool getOrder(uint64_t order_id, Order& out_order) const;
    
    double bestBid() const;
    uint64_t bestBidVolume() const;
    double bestAsk() const;
    uint64_t bestAskVolume() const;
    double midPrice() const;

    void printBook() const;
    string toJson() const;
};
