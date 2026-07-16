#include "engine/order_book.hpp"
#include "simulation/noise_trader.hpp"
#include "bot/market_maker.hpp"
#include "bot/imbalance_bot.hpp"
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

int main(int argc, char* argv[]) {
    auto book = make_shared<OrderBook>();
    uint64_t next_order_id = 1;
    string line;

    if (argc > 1) {
        // Parse CLI arguments into a single command line
        for (int i = 1; i < argc; ++i) {
            line += argv[i];
            if (i < argc - 1) line += " ";
        }
    } else {
        cout << "Order book CLI. Commands:\n"
             << "  buy <price> <qty>\n  sell <price> <qty>\n"
             << "  market buy <qty>\n  market sell <qty>\n"
             << "  ioc buy <price> <qty>\n  ioc sell <price> <qty>\n"
             << "  modify <order_id> <qty>\n  cancel <order_id>\n  book\n  bench\n  sim <steps> [avellaneda|imbalance|both]\n  serve [port] [bot1|bot2|both]\n  quit\n";
    }

    while (true) {
        if (argc == 1) {
            cout << "> ";
            if (!getline(cin, line)) break;
        }
        
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
        } else if (cmd == "modify") {
            uint64_t id, new_qty;
            if (!(iss >> id >> new_qty)) continue;
            if (book->modifyOrder(id, new_qty)) {
                cout << "  -> order " << id << " modified to qty " << new_qty << "\n";
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
            
            string mode = "both";
            bool do_plot = false;
            
            string token;
            while (iss >> token) {
                if (token == "-plot") {
                    do_plot = true;
                } else if (token == "avellaneda" || token == "imbalance" || token == "both") {
                    mode = token;
                }
            }

            NoiseTrader trader(book, 100.0, 1.0);
            MarketMaker bot(book, 1.0, 0.05, 1.5, 1.0);
            ImbalanceMarketMaker imb_bot(book, 1.0, 0.05, 1.5, 1.0);
            
            ofstream csv_file("sim_results.csv");
            if (mode == "both") {
                csv_file << "# Avellaneda-Stoikov (2008) vs Imbalance-Aware (2018)\n\n";
                csv_file << "Step,Inventory Avellaneda,PnL Avellaneda,Inventory Imbalance,PnL Imbalance\n\n";
            } else if (mode == "avellaneda") {
                csv_file << "# Avellaneda-Stoikov (2008)\n\n";
                csv_file << "Step,Inventory Avellaneda,PnL Avellaneda\n\n";
            } else if (mode == "imbalance") {
                csv_file << "# Imbalance-Aware (2018)\n\n";
                csv_file << "Step,Inventory Imbalance,PnL Imbalance\n\n";
            }

            cout << "  Simulating " << steps << " market events (Mode: " << mode << ")...\n";
            for (int i = 0; i < steps; ++i) {
                double dt = trader.tick();
                if (mode == "imbalance" || mode == "both") imb_bot.onTick(dt);
                if (mode == "avellaneda" || mode == "both") bot.onTick(dt);
                
                if (i % 5 == 0) {
                    if (mode == "both") {
                        csv_file << i << "," << bot.getInventory() << "," << bot.getPnl(book->midPrice()) << ","
                                 << imb_bot.getInventory() << "," << imb_bot.getPnl(book->midPrice()) << "\n";
                    } else if (mode == "avellaneda") {
                        csv_file << i << "," << bot.getInventory() << "," << bot.getPnl(book->midPrice()) << "\n";
                    } else if (mode == "imbalance") {
                        csv_file << i << "," << imb_bot.getInventory() << "," << imb_bot.getPnl(book->midPrice()) << "\n";
                    }
                }
            }
            csv_file.close();
            
            cout << "  Simulation complete. Results saved to sim_results.csv\n\n";
            cout << "  ================ FINAL RESULTS ================\n";
            if (mode == "avellaneda" || mode == "both") {
                cout << "  [Avellaneda-Stoikov 2008]\n";
                cout << "    Final PnL:       $" << bot.getPnl(book->midPrice()) << "\n";
                cout << "    Final Cash:      $" << bot.getCash() << "\n";
                cout << "    Final Inventory: " << bot.getInventory() << "\n";
            }
            if (mode == "imbalance" || mode == "both") {
                cout << "  [Imbalance-Aware 2018]\n";
                cout << "    Final PnL:       $" << imb_bot.getPnl(book->midPrice()) << "\n";
                cout << "    Final Cash:      $" << imb_bot.getCash() << "\n";
                cout << "    Final Inventory: " << imb_bot.getInventory() << "\n";
            }
            cout << "  ===============================================\n";
            
            ofstream append_csv("sim_results.csv", ios_base::app);
            append_csv << "\n# SUMMARY STATS\n\n";
            if (mode == "both" || mode == "avellaneda") {
                append_csv << "# Avellaneda-Stoikov PnL:       " << bot.getPnl(book->midPrice()) << "\n";
                append_csv << "# Avellaneda-Stoikov Cash:      " << bot.getCash() << "\n";
                append_csv << "# Avellaneda-Stoikov Inventory: " << bot.getInventory() << "\n\n";
            }
            if (mode == "both" || mode == "imbalance") {
                append_csv << "# Imbalance-Aware PnL:          " << imb_bot.getPnl(book->midPrice()) << "\n";
                append_csv << "# Imbalance-Aware Cash:         " << imb_bot.getCash() << "\n";
                append_csv << "# Imbalance-Aware Inventory:    " << imb_bot.getInventory() << "\n";
            }
            append_csv.close();
            
            if (do_plot) {
                cout << "  Generating plot...\n";
                system("./venv/bin/python3 scripts/plot_pnl.py &");
            }
        } else if (cmd == "serve") {
            int port = 8080;
            string mode = "both";
            string arg1;
            if (iss >> arg1) {
                if (arg1 == "bot1" || arg1 == "bot2" || arg1 == "both" || arg1 == "avellaneda" || arg1 == "imbalance") {
                    mode = arg1;
                    if (mode == "avellaneda") mode = "bot1";
                    if (mode == "imbalance") mode = "bot2";
                } else {
                    try { port = stoi(arg1); } catch(...) {}
                    string arg2;
                    if (iss >> arg2) {
                        mode = arg2;
                        if (mode == "avellaneda") mode = "bot1";
                        if (mode == "imbalance") mode = "bot2";
                    }
                }
            }
            
            atomic<bool> running{true};
            atomic<bool> is_bot1_paused{true};
            atomic<bool> is_bot2_paused{true};
            atomic<bool> is_gbm_paused{true};
            mutex state_mutex;
            
            NoiseTrader trader(book, 100.0, 1.0);
            MarketMaker bot(book, 1.0, 0.05, 1.5, 1.0);
            ImbalanceMarketMaker imb_bot(book, 1.0, 0.05, 1.5, 1.0);
            
            thread sim_thread([&]() {
                while (running) {
                    {
                        lock_guard<mutex> lock(state_mutex);
                        // Run 50 ticks per frame to speed up the simulation visually
                        for (int i = 0; i < 50; ++i) {
                            double dt = 0.01; // default dt if GBM is paused
                            if (!is_gbm_paused) {
                                dt = trader.tick();
                            }
                            if (!is_bot2_paused && (mode == "bot2" || mode == "both")) imb_bot.onTick(dt);
                            if (!is_bot1_paused && (mode == "bot1" || mode == "both")) bot.onTick(dt);
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

            svr.Post("/api/toggle_pause_bot1", [&](const httplib::Request& req, httplib::Response& res) {
                is_bot1_paused = !is_bot1_paused;
                if (is_bot1_paused) {
                    lock_guard<mutex> lock(state_mutex);
                    bot.cancelAll();
                }
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(is_bot1_paused ? "true" : "false", "application/json");
            });

            svr.Post("/api/toggle_pause_bot2", [&](const httplib::Request& req, httplib::Response& res) {
                is_bot2_paused = !is_bot2_paused;
                if (is_bot2_paused) {
                    lock_guard<mutex> lock(state_mutex);
                    imb_bot.cancelAll();
                }
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(is_bot2_paused ? "true" : "false", "application/json");
            });

            svr.Post("/api/toggle_pause_gbm", [&](const httplib::Request& req, httplib::Response& res) {
                is_gbm_paused = !is_gbm_paused;
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(is_gbm_paused ? "true" : "false", "application/json");
            });

            svr.Post("/api/set_pause_all", [&](const httplib::Request& req, httplib::Response& res) {
                string state = req.get_param_value("state");
                bool paused = (state == "true");
                is_bot1_paused = paused;
                is_bot2_paused = paused;
                is_gbm_paused = paused;
                if (paused) {
                    lock_guard<mutex> lock(state_mutex);
                    bot.cancelAll();
                    imb_bot.cancelAll();
                }
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(paused ? "true" : "false", "application/json");
            });

            svr.Post("/api/reset", [&](const httplib::Request& req, httplib::Response& res) {
                lock_guard<mutex> lock(state_mutex);
                is_bot1_paused = true;
                is_bot2_paused = true;
                is_gbm_paused = true;
                
                // Completely re-instantiate the environment
                book = make_shared<OrderBook>();
                trader = NoiseTrader(book, 100.0, 1.0);
                bot = MarketMaker(book, 1.0, 0.05, 1.5, 1.0);
                imb_bot = ImbalanceMarketMaker(book, 1.0, 0.05, 1.5, 1.0);
                
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("true", "application/json");
            });

            svr.Post("/api/reset_bot1", [&](const httplib::Request& req, httplib::Response& res) {
                lock_guard<mutex> lock(state_mutex);
                is_bot1_paused = true;
                bot = MarketMaker(book, 1.0, 0.05, 1.5, 1.0);
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("true", "application/json");
            });

            svr.Post("/api/reset_bot2", [&](const httplib::Request& req, httplib::Response& res) {
                lock_guard<mutex> lock(state_mutex);
                is_bot2_paused = true;
                imb_bot = ImbalanceMarketMaker(book, 1.0, 0.05, 1.5, 1.0);
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("true", "application/json");
            });

            svr.Post("/api/reset_gbm", [&](const httplib::Request& req, httplib::Response& res) {
                lock_guard<mutex> lock(state_mutex);
                is_gbm_paused = true;
                trader = NoiseTrader(book, 100.0, 1.0);
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("true", "application/json");
            });

            svr.Post("/api/toggle_hawkes", [&](const httplib::Request& req, httplib::Response& res) {
                lock_guard<mutex> lock(state_mutex);
                trader.setHawkes(!trader.getHawkes());
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(trader.getHawkes() ? "true" : "false", "application/json");
            });

            svr.Get("/api/state", [&](const httplib::Request& req, httplib::Response& res) {
                lock_guard<mutex> lock(state_mutex);
                string book_json = book->toJson();
                
                book_json.pop_back(); // Remove closing brace
                stringstream ss;
                ss << book_json << ", "
                   << "\"mid_price\": " << book->midPrice() << ", "
                   << "\"bot1_inventory\": " << bot.getInventory() << ", "
                   << "\"bot1_cash\": " << bot.getCash() << ", "
                   << "\"bot1_pnl\": " << bot.getPnl(book->midPrice()) << ", "
                   << "\"bot1_bid\": " << bot.getActiveBidPrice() << ", "
                   << "\"bot1_ask\": " << bot.getActiveAskPrice() << ", "
                   << "\"bot1_bid_qty\": " << bot.getActiveBidQty() << ", "
                   << "\"bot1_ask_qty\": " << bot.getActiveAskQty() << ", "
                   << "\"bot2_inventory\": " << imb_bot.getInventory() << ", "
                   << "\"bot2_cash\": " << imb_bot.getCash() << ", "
                   << "\"bot2_pnl\": " << imb_bot.getPnl(book->midPrice()) << ", "
                   << "\"bot2_bid\": " << imb_bot.getActiveBidPrice() << ", "
                   << "\"bot2_ask\": " << imb_bot.getActiveAskPrice() << ", "
                   << "\"bot2_bid_qty\": " << imb_bot.getActiveBidQty() << ", "
                   << "\"bot2_ask_qty\": " << imb_bot.getActiveAskQty() << ", "
                   << "\"bot2_microprice\": " << imb_bot.getMicroPrice() << ", "
                   << "\"active_mode\": \"" << mode << "\", "
                   << "\"is_bot1_paused\": " << (is_bot1_paused ? "true" : "false") << ", "
                   << "\"is_bot2_paused\": " << (is_bot2_paused ? "true" : "false") << ", "
                   << "\"is_gbm_paused\": " << (is_gbm_paused ? "true" : "false") << ", "
                   << "\"is_hawkes_enabled\": " << (trader.getHawkes() ? "true" : "false") << "}";
                   
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
        
        // If running as a one-off command from CLI args, exit immediately after execution
        if (argc > 1) break;
    }
    return 0;
}
