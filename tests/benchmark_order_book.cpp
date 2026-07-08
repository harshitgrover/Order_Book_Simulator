#include <benchmark/benchmark.h>
#include "engine/order_book.hpp"
#include "engine/order.hpp"
#include <vector>
#include <algorithm>
// Benchmark adding limit orders
static void BM_OrderBookAddLimitOrder(benchmark::State& state) {
    OrderBook book;
    uint64_t order_id = 1;

    for (auto _ : state) {
        Order o;
        o.order_id = order_id++;
        o.side = (order_id % 2 == 0) ? Side::BUY : Side::SELL;
        o.type = OrderType::LIMIT;
        o.price = 100.0 + (order_id % 20) - 10.0;
        o.quantity = 10;
        o.timestamp = 0;

        book.addLimitOrder(o);
    }
}

// Register the benchmark, measuring wall clock time
BENCHMARK(BM_OrderBookAddLimitOrder)
    ->Unit(benchmark::kNanosecond)
    ->ComputeStatistics("p99", [](const std::vector<double>& v) -> double {
        if (v.empty()) return 0;
        auto copy = v;
        std::sort(copy.begin(), copy.end());
        return copy[static_cast<size_t>(copy.size() * 0.99)];
    });
