#pragma once

#include "engine/order_book.hpp"
#include "simulation/price_process.hpp"
#include <random>
#include <memory>

using namespace std;

class NoiseTrader {
private:
    shared_ptr<OrderBook> order_book;
    GeometricBrownianMotion gbm;
    double base_rate;
    uint64_t next_order_id;
    
    mt19937 generator;
    uniform_real_distribution<double> uniform_dist;

    // Hawkes process state
    double buy_excitement = 0.0;
    double sell_excitement = 0.0;
    double alpha = 8.0;
    double beta = 10.0;

public:
    NoiseTrader(shared_ptr<OrderBook> book, double start_price, double arrival_rate);

    double tick();
};
