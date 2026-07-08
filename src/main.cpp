#include "engine/order_book.hpp"
#include "simulation/noise_trader.hpp"
#include "bot/market_maker.hpp"
#include "httplib.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <algorithm>

using namespace std;
using namespace std::chrono;

void runBenchmark() {
    OrderBook book;
    const int NUM_ORDERS = 1000000;
    cout << "  Running benchmark with " << NUM_ORDERS << " limit orders...\n";
    
    vector<long long> latencies;
    latencies.reserve(NUM_ORDERS);

    auto start = high_resolution_clock::now();
    for (int i = 0; i < NUM_ORDERS; ++i) {
        Order o;
        o.order_id = i + 1;
        o.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        o.type = OrderType::LIMIT;
        o.price = 100.0 + (i % 20) - 10.0;
        o.quantity = 10;
        o.timestamp = 0;
        
        auto t1 = high_resolution_clock::now();
        book.addLimitOrder(o);
        auto t2 = high_resolution_clock::now();
        
        latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
    }
    auto end = high_resolution_clock::now();
    duration<double> diff = end - start;
    
    sort(latencies.begin(), latencies.end());
    
    cout << "  Time taken (Total): " << diff.count() << " s\n";
    cout << "  Throughput: " << (NUM_ORDERS / diff.count()) << " orders/sec\n";
    cout << "  Latency p50: " << latencies[NUM_ORDERS * 0.50] << " ns\n";
    cout << "  Latency p90: " << latencies[NUM_ORDERS * 0.90] << " ns\n";
    cout << "  Latency p99: " << latencies[NUM_ORDERS * 0.99] << " ns\n";
    cout << "  Latency p99.9: " << latencies[NUM_ORDERS * 0.999] << " ns\n";
}

int main() {
    auto book = make_shared<OrderBook>();
    uint64_t next_order_id = 1;
    string line;

    cout << "Order book CLI. Commands:\n"
         << "  buy <price> <qty>\n  sell <price> <qty>\n"
         << "  market buy <qty>\n  market sell <qty>\n"
         << "  ioc buy <price> <qty>\n  ioc sell <price> <qty>\n"
         << "  cancel <order_id>\n  book\n  bench\n  sim <steps>\n  quit\n";

    while (true) {
        cout << "> ";
        if (!getline(cin, line)) break;
        
        istringstream iss(line);
        string cmd; 
        iss >> cmd;

        if (cmd == "buy" || cmd == "sell") {
            double price; 
            uint64_t qty;
            if (!(iss >> price >> qty)) continue;
            
            Order order{next_order_id++, (cmd == "buy") ? Side::BUY : Side::SELL, OrderType::LIMIT, price, qty, 0};
            auto trades = book->addLimitOrder(order);
            cout << "  -> limit order id " << order.order_id << " placed\n";
            for (auto& t : trades)
                cout << "  TRADE: " << t.quantity << " @ " << t.price << "\n";
        } else if (cmd == "market") {
            string side_str;
            uint64_t qty;
            if (!(iss >> side_str >> qty)) continue;
            Order order{next_order_id++, (side_str == "buy") ? Side::BUY : Side::SELL, OrderType::MARKET, 0.0, qty, 0};
            auto trades = book->addMarketOrder(order);
            cout << "  -> market order id " << order.order_id << " placed\n";
            for (auto& t : trades)
                cout << "  TRADE: " << t.quantity << " @ " << t.price << "\n";
        } else if (cmd == "ioc") {
            string side_str;
            double price;
            uint64_t qty;
            if (!(iss >> side_str >> price >> qty)) continue;
            Order order{next_order_id++, (side_str == "buy") ? Side::BUY : Side::SELL, OrderType::IOC, price, qty, 0};
            auto trades = book->addIocOrder(order);
            cout << "  -> ioc order id " << order.order_id << " placed\n";
            for (auto& t : trades)
                cout << "  TRADE: " << t.quantity << " @ " << t.price << "\n";
        } else if (cmd == "cancel") {
            uint64_t id; 
            if (!(iss >> id)) continue;
            if (book->cancelOrder(id)) {
                cout << "  -> order " << id << " cancelled\n";
            } else {
                cout << "  -> order " << id << " not found or already filled\n";
            }
        } else if (cmd == "book") {
            book->printBook();
        } else if (cmd == "bench") {
            runBenchmark();
        } else if (cmd == "sim") {
            int steps;
            if (!(iss >> steps)) continue;
            NoiseTrader trader(book, 100.0, 1.0);
            MarketMaker bot(book, 1.0, 0.05, 1.5, 1.0);
            
            ofstream csv_file("sim_results.csv");
            csv_file << "step,inventory,pnl\n";

            cout << "  Simulating " << steps << " market events with Avellaneda-Stoikov MM...\n";
            for (int i = 0; i < steps; ++i) {
                double dt = trader.tick();
                bot.onTick(dt);
                if (i % 5 == 0) {
                    csv_file << i << "," << bot.getInventory() << "," << bot.getPnl(book->midPrice()) << "\n";
                }
            }
            csv_file.close();
            
            cout << "  Simulation complete. Results saved to sim_results.csv\n";
            cout << "  MM Final Inventory: " << bot.getInventory() << "\n";
        } else if (cmd == "serve") {
            int port = 8080;
            if (iss >> port) {} // Optional port param
            
            atomic<bool> running{true};
            mutex state_mutex;
            
            NoiseTrader trader(book, 100.0, 1.0);
            MarketMaker bot(book, 1.0, 0.05, 1.5, 1.0);
            
            thread sim_thread([&]() {
                while (running) {
                    {
                        lock_guard<mutex> lock(state_mutex);
                        // Run 50 ticks per frame to speed up the simulation visually
                        for (int i = 0; i < 50; ++i) {
                            double dt = trader.tick();
                            bot.onTick(dt);
                        }
                    }
                    this_thread::sleep_for(milliseconds(100)); // 10 frames per second = 500 ticks/sec
                }
            });
            
            httplib::Server svr;
            
            // Enable CORS
            svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type");
            });

            svr.Get("/api/state", [&](const httplib::Request& req, httplib::Response& res) {
                lock_guard<mutex> lock(state_mutex);
                string book_json = book->toJson();
                
                book_json.pop_back(); // Remove closing brace
                stringstream ss;
                ss << book_json << ", "
                   << "\"bot_inventory\": " << bot.getInventory() << ", "
                   << "\"bot_cash\": " << bot.getCash() << ", "
                   << "\"bot_pnl\": " << bot.getPnl(book->midPrice()) << ", "
                   << "\"bot_bid\": " << bot.getActiveBidPrice() << ", "
                   << "\"bot_ask\": " << bot.getActiveAskPrice() << ", "
                   << "\"bot_bid_qty\": " << bot.getActiveBidQty() << ", "
                   << "\"bot_ask_qty\": " << bot.getActiveAskQty() << "}";
                   
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(ss.str(), "application/json");
            });
            
            svr.set_mount_point("/", "ui");
            
            cout << "  Starting simulation and HTTP server on port " << port << "...\n";
            svr.listen("0.0.0.0", port);
            
            running = false;
            sim_thread.join();
        } else if (cmd == "quit") {
            break;
        } else if (!cmd.empty()) {
            cout << "  unknown command\n";
        }
    }
    return 0;
}
