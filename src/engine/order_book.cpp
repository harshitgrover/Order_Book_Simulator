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

OrderBook::OrderBook() : bids(MAX_PRICE_TICKS), asks(MAX_PRICE_TICKS), best_bid_idx(0), best_ask_idx(MAX_PRICE_TICKS - 1) {
    // Initialize Object Pool
    node_pool.resize(1000000);
    for (uint32_t i = 1; i < node_pool.size() - 1; ++i) {
        node_pool[i].next = i + 1;
    }
    node_pool.back().next = 0;
    first_free = 1;
}

uint32_t OrderBook::allocateNode() {
    if (first_free == 0) {
        // Pool exhausted, chunk allocate another million
        uint32_t old_size = node_pool.size();
        node_pool.resize(old_size + 1000000);
        for (uint32_t i = old_size; i < node_pool.size() - 1; ++i) {
            node_pool[i].next = i + 1;
        }
        node_pool.back().next = 0;
        first_free = old_size;
    }
    uint32_t node_idx = first_free;
    first_free = node_pool[node_idx].next;
    node_pool[node_idx].next = 0;
    node_pool[node_idx].prev = 0;
    return node_idx;
}

void OrderBook::freeNode(uint32_t node_idx) {
    node_pool[node_idx].next = first_free;
    first_free = node_idx;
}

static void appendToLevel(PriceLevel& level, uint32_t node_idx, vector<OrderNode>& node_pool) {
    if (level.head == 0) {
        level.head = node_idx;
        level.tail = node_idx;
        node_pool[node_idx].prev = 0;
        node_pool[node_idx].next = 0;
    } else {
        node_pool[level.tail].next = node_idx;
        node_pool[node_idx].prev = level.tail;
        node_pool[node_idx].next = 0;
        level.tail = node_idx;
    }
}

static void removeFromLevel(PriceLevel& level, uint32_t node_idx, vector<OrderNode>& node_pool) {
    uint32_t prev_idx = node_pool[node_idx].prev;
    uint32_t next_idx = node_pool[node_idx].next;

    if (prev_idx != 0) {
        node_pool[prev_idx].next = next_idx;
    } else {
        level.head = next_idx;
    }

    if (next_idx != 0) {
        node_pool[next_idx].prev = prev_idx;
    } else {
        level.tail = prev_idx;
    }
}

vector<Trade> OrderBook::addLimitOrder(Order order) {
    if (order.timestamp == 0) [[unlikely]] order.timestamp = getCurrentTime();
    vector<Trade> trades;

    if (order.side == Side::BUY) {
        while (order.quantity > 0 && best_ask_idx < MAX_PRICE_TICKS) {
            if (best_ask_idx > priceToIndex(order.price)) break; // Ask is too high

            auto& level = asks[best_ask_idx];
            uint32_t curr_node_idx = level.head;
            
            while (order.quantity > 0 && curr_node_idx != 0) {
                Order& resting_order = node_pool[curr_node_idx].order;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    order.order_id,
                    resting_order.order_id,
                    asks[best_ask_idx].price,
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                uint32_t next_node_idx = node_pool[curr_node_idx].next;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    removeFromLevel(level, curr_node_idx, node_pool);
                    freeNode(curr_node_idx);
                }
                curr_node_idx = next_node_idx;
            }

            if (level.head == 0) {
                updateBestAskIdxUp();
            }
        }

        if (order.quantity > 0) [[likely]] {
            size_t idx = priceToIndex(order.price);
            auto& level = bids[idx];
            level.price = order.price;
            
            uint32_t new_node_idx = allocateNode();
            node_pool[new_node_idx].order = std::move(order);
            appendToLevel(level, new_node_idx, node_pool);
            
            order_index[node_pool[new_node_idx].order.order_id] = {Side::BUY, idx, new_node_idx};
            
            if (idx > best_bid_idx) {
                best_bid_idx = idx;
            }
        }

    } else {
        while (order.quantity > 0 && best_bid_idx > 0) {
            if (best_bid_idx < priceToIndex(order.price)) break;

            auto& level = bids[best_bid_idx];
            uint32_t curr_node_idx = level.head;
            
            while (order.quantity > 0 && curr_node_idx != 0) {
                Order& resting_order = node_pool[curr_node_idx].order;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    resting_order.order_id,
                    order.order_id,
                    bids[best_bid_idx].price,
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                uint32_t next_node_idx = node_pool[curr_node_idx].next;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    removeFromLevel(level, curr_node_idx, node_pool);
                    freeNode(curr_node_idx);
                }
                curr_node_idx = next_node_idx;
            }

            if (level.head == 0) {
                updateBestBidIdxDown();
            }
        }

        if (order.quantity > 0) [[likely]] {
            size_t idx = priceToIndex(order.price);
            auto& level = asks[idx];
            level.price = order.price;
            
            uint32_t new_node_idx = allocateNode();
            node_pool[new_node_idx].order = std::move(order);
            appendToLevel(level, new_node_idx, node_pool);
            
            order_index[node_pool[new_node_idx].order.order_id] = {Side::SELL, idx, new_node_idx};

            if (idx < best_ask_idx) {
                best_ask_idx = idx;
            }
        }
    }

    return trades;
}

bool OrderBook::cancelOrder(uint64_t order_id) {
    auto it = order_index.find(order_id);
    if (it == order_index.end()) return false;

    OrderIndexEntry& entry = it->second;
    uint32_t node_idx = entry.node_idx;
    
    if (entry.side == Side::BUY) {
        auto& level = bids[entry.price_idx];
        removeFromLevel(level, node_idx, node_pool);
        if (level.head == 0 && entry.price_idx == best_bid_idx) {
            updateBestBidIdxDown();
        }
    } else {
        auto& level = asks[entry.price_idx];
        removeFromLevel(level, node_idx, node_pool);
        if (level.head == 0 && entry.price_idx == best_ask_idx) {
            updateBestAskIdxUp();
        }
    }
    
    freeNode(node_idx);
    order_index.erase(it);
    return true;
}

bool OrderBook::modifyOrder(uint64_t order_id, uint64_t new_qty) {
    auto it = order_index.find(order_id);
    if (it == order_index.end()) return false;

    OrderIndexEntry& entry = it->second;
    uint32_t node_idx = entry.node_idx;
    Order& order = node_pool[node_idx].order;

    if (new_qty < order.quantity) {
        // Queue Priority Preserved: Just update in-place!
        order.quantity = new_qty;
        return true;
    } else if (new_qty > order.quantity) {
        // Queue Priority Lost: Must move to back of the queue
        // We simulate this by removing and re-appending
        Order updated_order = order;
        updated_order.quantity = new_qty;
        cancelOrder(order_id);
        addLimitOrder(updated_order);
        return true;
    }
    
    return true; // Unchanged
}

vector<Trade> OrderBook::addMarketOrder(Order order) {
    if (order.timestamp == 0) [[unlikely]] order.timestamp = getCurrentTime();
    vector<Trade> trades;

    if (order.side == Side::BUY) {
        while (order.quantity > 0 && best_ask_idx < MAX_PRICE_TICKS) {
            auto& level = asks[best_ask_idx];
            if (level.head == 0) {
                updateBestAskIdxUp();
                continue;
            }

            uint32_t curr_node_idx = level.head;
            while (order.quantity > 0 && curr_node_idx != 0) {
                Order& resting_order = node_pool[curr_node_idx].order;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    order.order_id,
                    resting_order.order_id,
                    asks[best_ask_idx].price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;
                
                uint32_t next_node_idx = node_pool[curr_node_idx].next;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    removeFromLevel(level, curr_node_idx, node_pool);
                    freeNode(curr_node_idx);
                }
                curr_node_idx = next_node_idx;
            }

            if (level.head == 0) {
                updateBestAskIdxUp();
            }
        }
    } else {
        while (order.quantity > 0 && best_bid_idx > 0) {
            auto& level = bids[best_bid_idx];
            if (level.head == 0) {
                updateBestBidIdxDown();
                continue;
            }

            uint32_t curr_node_idx = level.head;
            while (order.quantity > 0 && curr_node_idx != 0) {
                Order& resting_order = node_pool[curr_node_idx].order;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    resting_order.order_id,
                    order.order_id,
                    bids[best_bid_idx].price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;

                uint32_t next_node_idx = node_pool[curr_node_idx].next;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    removeFromLevel(level, curr_node_idx, node_pool);
                    freeNode(curr_node_idx);
                }
                curr_node_idx = next_node_idx;
            }

            if (level.head == 0) {
                updateBestBidIdxDown();
            }
        }
    }

    return trades;
}

vector<Trade> OrderBook::addIocOrder(Order order) {
    if (order.timestamp == 0) [[unlikely]] order.timestamp = getCurrentTime();
    vector<Trade> trades;

    if (order.side == Side::BUY) {
        while (order.quantity > 0 && best_ask_idx < MAX_PRICE_TICKS) {
            if (best_ask_idx > priceToIndex(order.price)) break;

            auto& level = asks[best_ask_idx];
            uint32_t curr_node_idx = level.head;

            while (order.quantity > 0 && curr_node_idx != 0) {
                Order& resting_order = node_pool[curr_node_idx].order;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    order.order_id,
                    resting_order.order_id,
                    asks[best_ask_idx].price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;
                
                uint32_t next_node_idx = node_pool[curr_node_idx].next;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    removeFromLevel(level, curr_node_idx, node_pool);
                    freeNode(curr_node_idx);
                }
                curr_node_idx = next_node_idx;
            }

            if (level.head == 0) {
                updateBestAskIdxUp();
            }
        }
    } else {
        while (order.quantity > 0 && best_bid_idx > 0) {
            if (best_bid_idx < priceToIndex(order.price)) break;

            auto& level = bids[best_bid_idx];
            uint32_t curr_node_idx = level.head;

            while (order.quantity > 0 && curr_node_idx != 0) {
                Order& resting_order = node_pool[curr_node_idx].order;

                uint64_t trade_qty = min(order.quantity, resting_order.quantity);
                trades.push_back({
                    resting_order.order_id,
                    order.order_id,
                    bids[best_bid_idx].price, 
                    trade_qty,
                    getCurrentTime()
                });

                order.quantity -= trade_qty;
                resting_order.quantity -= trade_qty;
                
                uint32_t next_node_idx = node_pool[curr_node_idx].next;

                if (resting_order.quantity == 0) {
                    order_index.erase(resting_order.order_id);
                    removeFromLevel(level, curr_node_idx, node_pool);
                    freeNode(curr_node_idx);
                }
                curr_node_idx = next_node_idx;
            }

            if (level.head == 0) {
                updateBestBidIdxDown();
            }
        }
    }

    return trades;
}

bool OrderBook::getOrder(uint64_t order_id, Order& out_order) const {
    auto it = order_index.find(order_id);
    if (it != order_index.end()) {
        out_order = node_pool[it->second.node_idx].order;
        return true;
    }
    return false;
}

double OrderBook::bestBid() const {
    if (best_bid_idx == 0 && bids[0].head == 0) return 0.0;
    return bids[best_bid_idx].price;
}

uint64_t OrderBook::bestBidVolume() const {
    if (best_bid_idx == 0 && bids[0].head == 0) return 0;
    uint64_t vol = 0;
    uint32_t curr = bids[best_bid_idx].head;
    while(curr != 0) {
        vol += node_pool[curr].order.quantity;
        curr = node_pool[curr].next;
    }
    return vol;
}

double OrderBook::bestAsk() const {
    if (best_ask_idx == MAX_PRICE_TICKS - 1 && asks[MAX_PRICE_TICKS - 1].head == 0) return 0.0;
    return asks[best_ask_idx].price;
}

uint64_t OrderBook::bestAskVolume() const {
    if (best_ask_idx == MAX_PRICE_TICKS - 1 && asks[MAX_PRICE_TICKS - 1].head == 0) return 0;
    uint64_t vol = 0;
    uint32_t curr = asks[best_ask_idx].head;
    while(curr != 0) {
        vol += node_pool[curr].order.quantity;
        curr = node_pool[curr].next;
    }
    return vol;
}

double OrderBook::midPrice() const {
    double bb = bestBid();
    double ba = bestAsk();
    if (bb == 0.0 || ba == 0.0) return 0.0;
    return (bb + ba) / 2.0;
}

void OrderBook::printBook() const {
    cout << "  ASKS:\n";
    for (size_t i = MAX_PRICE_TICKS - 1; i >= best_ask_idx && i != static_cast<size_t>(-1); --i) {
        if (asks[i].head != 0) {
            uint64_t total_qty = 0;
            uint32_t curr = asks[i].head;
            while(curr != 0) {
                total_qty += node_pool[curr].order.quantity;
                curr = node_pool[curr].next;
            }
            cout << "    " << asks[i].price << " x " << total_qty << "\n";
        }
    }
    
    cout << "  BIDS:\n";
    for (size_t i = best_bid_idx; i > 0; --i) {
        if (bids[i].head != 0) {
            uint64_t total_qty = 0;
            uint32_t curr = bids[i].head;
            while(curr != 0) {
                total_qty += node_pool[curr].order.quantity;
                curr = node_pool[curr].next;
            }
            cout << "    " << bids[i].price << " x " << total_qty << "\n";
        }
    }
}

string OrderBook::toJson() const {
    stringstream ss;
    ss << "{";
    
    // Asks
    ss << "\"asks\": [";
    int count = 0;
    for (size_t i = best_ask_idx; i < MAX_PRICE_TICKS && count < 15; ++i) {
        if (asks[i].head != 0) {
            if (count > 0) ss << ", ";
            uint64_t total_qty = 0;
            uint32_t curr = asks[i].head;
            while(curr != 0) {
                total_qty += node_pool[curr].order.quantity;
                curr = node_pool[curr].next;
            }
            ss << "[" << asks[i].price << ", " << total_qty << "]";
            ++count;
        }
    }
    ss << "], ";
    
    // Bids
    ss << "\"bids\": [";
    count = 0;
    for (size_t i = best_bid_idx; i > 0 && count < 15; --i) {
        if (bids[i].head != 0) {
            if (count > 0) ss << ", ";
            uint64_t total_qty = 0;
            uint32_t curr = bids[i].head;
            while(curr != 0) {
                total_qty += node_pool[curr].order.quantity;
                curr = node_pool[curr].next;
            }
            ss << "[" << bids[i].price << ", " << total_qty << "]";
            ++count;
        }
    }
    ss << "]";
    
    ss << "}";
    return ss.str();
}

