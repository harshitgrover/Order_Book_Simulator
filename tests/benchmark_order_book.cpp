#include <benchmark/benchmark.h>
#include "engine/order_book.hpp"
#include "engine/order.hpp"
#include "bot/market_maker.hpp"
#include "bot/imbalance_bot.hpp"
#include <vector>
#include <memory>
#include <map>
#include <list>
#include <algorithm>

// Benchmark adding limit orders to the new O(1) Flat Book (std::vector)
static void BM_OrderBook(benchmark::State& state) {
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
BENCHMARK(BM_OrderBook) // This is the modern vector Flat Book
    ->Unit(benchmark::kNanosecond)
    ->ComputeStatistics("p99", [](const std::vector<double>& v) -> double {
        if (v.empty()) return 0;
        auto copy = v;
        std::sort(copy.begin(), copy.end());
        return copy[static_cast<size_t>(copy.size() * 0.99)];
    });

// Benchmark MarketMaker (Avellaneda-Stoikov) processing time
static void BM_AvellanedaStoikovBot(benchmark::State& state) {
    auto book = std::make_shared<OrderBook>();
    
    Order bid{1, Side::BUY, OrderType::LIMIT, 100.0, 10, 0};
    Order ask{2, Side::SELL, OrderType::LIMIT, 101.0, 10, 0};
    book->addLimitOrder(bid);
    book->addLimitOrder(ask);

    MarketMaker bot(book, 1.0, 0.05, 1.5, 1.0);

    for (auto _ : state) {
        bot.onTick(1.0);
    }
}
BENCHMARK(BM_AvellanedaStoikovBot)->Unit(benchmark::kNanosecond);

// Benchmark ImbalanceMarketMaker processing time
static void BM_ImbalanceAwareBot(benchmark::State& state) {
    auto book = std::make_shared<OrderBook>();
    
    Order bid{1, Side::BUY, OrderType::LIMIT, 100.0, 10, 0};
    Order ask{2, Side::SELL, OrderType::LIMIT, 101.0, 10, 0};
    book->addLimitOrder(bid);
    book->addLimitOrder(ask);

    ImbalanceMarketMaker bot(book, 1.0, 0.05, 1.5, 1.0);

    for (auto _ : state) {
        bot.onTick(1.0);
    }
}
BENCHMARK(BM_ImbalanceAwareBot)->Unit(benchmark::kNanosecond);
