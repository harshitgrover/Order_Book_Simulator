#pragma once

#include <cstdint>

using namespace std;

enum class Side { BUY, SELL };
enum class OrderType { LIMIT, MARKET, IOC, CANCEL };

struct Order {
    uint64_t order_id;
    Side side;
    OrderType type;
    double price;
    uint64_t quantity;
    uint64_t timestamp;
};

struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    double price;
    uint64_t quantity;
    uint64_t timestamp;
};
