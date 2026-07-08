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
    double arrival_rate;
    uint64_t next_order_id;
    
    mt19937 generator;
    exponential_distribution<double> exponential_dist;
    uniform_real_distribution<double> uniform_dist;

public:
    NoiseTrader(shared_ptr<OrderBook> book, double start_price, double arrival_rate);

    double tick();
};
