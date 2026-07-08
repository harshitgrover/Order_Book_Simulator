#include "engine/order_book.hpp"
#include <chrono>
#include <sstream>

using namespace std;
using namespace std::chrono;

uint64_t getCurrentTime() {
    return duration_cast<nanoseconds>(
               system_clock::now().time_since_epoch())
        .count();
}

vector<Trade> OrderBook::addLimitOrder(Order order) {
    if (order.timestamp == 0) order.timestamp = getCurrentTime();
    vector<Trade> trades;

    if (order.side == Side::BUY) {
        while (order.quantity > 0 && !asks.empty()) {
            auto best_ask_it = asks.begin();
            double best_ask_price = best_ask_it->first;

            if (best_ask_price > order.price) break;

            auto& level_orders = best_ask_it->second.orders;
            while (order.quantity > 0 && !level_orders.empty()) {
                auto resting_order_it = level_orders.begin();
                Order& resting_order = *resting_order_it;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    order.order_id,
                    resting_order.order_id,
                    best_ask_price,
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    level_orders.pop_front();
                }
            }

            if (level_orders.empty()) {
                asks.erase(best_ask_it);
            }
        }

        if (order.quantity > 0) {
            auto& level = bids[order.price];
            level.price = order.price;
            level.orders.push_back(order);
            auto it = level.orders.end();
            --it;
            order_index[order.order_id] = {Side::BUY, order.price, it};
        }

    } else {
        while (order.quantity > 0 && !bids.empty()) {
            auto best_bid_it = bids.begin();
            double best_bid_price = best_bid_it->first;

            if (best_bid_price < order.price) break;

            auto& level_orders = best_bid_it->second.orders;
            while (order.quantity > 0 && !level_orders.empty()) {
                auto resting_order_it = level_orders.begin();
                Order& resting_order = *resting_order_it;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    resting_order.order_id,
                    order.order_id,
                    best_bid_price,
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    level_orders.pop_front();
                }
            }

            if (level_orders.empty()) {
                bids.erase(best_bid_it);
            }
        }

        if (order.quantity > 0) {
            auto& level = asks[order.price];
            level.price = order.price;
            level.orders.push_back(order);
            auto it = level.orders.end();
            --it;
            order_index[order.order_id] = {Side::SELL, order.price, it};
        }
    }

    return trades;
}

bool OrderBook::cancelOrder(uint64_t order_id) {
    auto it = order_index.find(order_id);
    if (it == order_index.end()) return false;

    OrderIndexEntry& entry = it->second;
    if (entry.side == Side::BUY) {
        auto map_it = bids.find(entry.price);
        if (map_it != bids.end()) {
            map_it->second.orders.erase(entry.iterator);
            if (map_it->second.orders.empty()) {
                bids.erase(map_it);
            }
        }
    } else {
        auto map_it = asks.find(entry.price);
        if (map_it != asks.end()) {
            map_it->second.orders.erase(entry.iterator);
            if (map_it->second.orders.empty()) {
                asks.erase(map_it);
            }
        }
    }
    order_index.erase(it);
    return true;
}

vector<Trade> OrderBook::addMarketOrder(Order order) {
    if (order.timestamp == 0) order.timestamp = getCurrentTime();
    vector<Trade> trades;

    if (order.side == Side::BUY) {
        while (order.quantity > 0 && !asks.empty()) {
            auto best_ask_it = asks.begin();
            double best_ask_price = best_ask_it->first;
            auto& level_orders = best_ask_it->second.orders;

            while (order.quantity > 0 && !level_orders.empty()) {
                auto resting_order_it = level_orders.begin();
                Order& resting_order = *resting_order_it;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    order.order_id,
                    resting_order.order_id,
                    best_ask_price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    level_orders.pop_front();
                }
            }

            if (level_orders.empty()) {
                asks.erase(best_ask_it);
            }
        }
    } else {
        while (order.quantity > 0 && !bids.empty()) {
            auto best_bid_it = bids.begin();
            double best_bid_price = best_bid_it->first;
            auto& level_orders = best_bid_it->second.orders;

            while (order.quantity > 0 && !level_orders.empty()) {
                auto resting_order_it = level_orders.begin();
                Order& resting_order = *resting_order_it;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    resting_order.order_id,
                    order.order_id,
                    best_bid_price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    level_orders.pop_front();
                }
            }

            if (level_orders.empty()) {
                bids.erase(best_bid_it);
            }
        }
    }

    return trades;
}

vector<Trade> OrderBook::addIocOrder(Order order) {
    if (order.timestamp == 0) order.timestamp = getCurrentTime();
    vector<Trade> trades;

    if (order.side == Side::BUY) {
        while (order.quantity > 0 && !asks.empty()) {
            auto best_ask_it = asks.begin();
            double best_ask_price = best_ask_it->first;

            if (best_ask_price > order.price) break;

            auto& level_orders = best_ask_it->second.orders;

            while (order.quantity > 0 && !level_orders.empty()) {
                auto resting_order_it = level_orders.begin();
                Order& resting_order = *resting_order_it;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    order.order_id,
                    resting_order.order_id,
                    best_ask_price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    level_orders.pop_front();
                }
            }

            if (level_orders.empty()) {
                asks.erase(best_ask_it);
            }
        }
    } else {
        while (order.quantity > 0 && !bids.empty()) {
            auto best_bid_it = bids.begin();
            double best_bid_price = best_bid_it->first;

            if (best_bid_price < order.price) break;

            auto& level_orders = best_bid_it->second.orders;

            while (order.quantity > 0 && !level_orders.empty()) {
                auto resting_order_it = level_orders.begin();
                Order& resting_order = *resting_order_it;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    resting_order.order_id,
                    order.order_id,
                    best_bid_price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    level_orders.pop_front();
                }
            }

            if (level_orders.empty()) {
                bids.erase(best_bid_it);
            }
        }
    }

    return trades;
}

bool OrderBook::getOrder(uint64_t order_id, Order& out_order) const {
    auto it = order_index.find(order_id);
    if (it != order_index.end()) {
        out_order = *(it->second.iterator);
        return true;
    }
    return false;
}

double OrderBook::bestBid() const {
    if (bids.empty()) return 0.0;
    return bids.begin()->first;
}

double OrderBook::bestAsk() const {
    if (asks.empty()) return 0.0;
    return asks.begin()->first;
}

double OrderBook::midPrice() const {
    if (bids.empty() || asks.empty()) return 0.0;
    return (bestBid() + bestAsk()) / 2.0;
}

void OrderBook::printBook() const {
    cout << "  ASKS:\n";
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        uint64_t total_qty = 0;
        for (const auto& o : it->second.orders) total_qty += o.quantity;
        cout << "    " << it->first << " x " << total_qty << "\n";
    }
    
    cout << "  BIDS:\n";
    for (auto it = bids.begin(); it != bids.end(); ++it) {
        uint64_t total_qty = 0;
        for (const auto& o : it->second.orders) total_qty += o.quantity;
        cout << "    " << it->first << " x " << total_qty << "\n";
    }
}

string OrderBook::toJson() const {
    stringstream ss;
    ss << "{";
    
    // Asks
    ss << "\"asks\": [";
    int count = 0;
    for (auto it = asks.begin(); it != asks.end() && count < 15; ++it, ++count) {
        if (count > 0) ss << ", ";
        uint64_t total_qty = 0;
        for (const auto& o : it->second.orders) total_qty += o.quantity;
        ss << "[" << it->first << ", " << total_qty << "]";
    }
    ss << "], ";
    
    // Bids
    ss << "\"bids\": [";
    count = 0;
    for (auto it = bids.begin(); it != bids.end() && count < 15; ++it, ++count) {
        if (count > 0) ss << ", ";
        uint64_t total_qty = 0;
        for (const auto& o : it->second.orders) total_qty += o.quantity;
        ss << "[" << it->first << ", " << total_qty << "]";
    }
    ss << "]";
    
    ss << "}";
    return ss.str();
}
