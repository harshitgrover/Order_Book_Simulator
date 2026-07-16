# Limit Order Book & Trading Strategy Comparison

A high-performance Limit Order Book (LOB) matching engine written in C++, complete with stochastic market maker bots (Avellaneda-Stoikov Bot & Stoikov Micro-Price Bot) simulating real-world HFT liquidity provision, and a live web dashboard.

## 🚀 High-Frequency Architecture

- **Hard Price Cap:** Due to the cache-aligned Flat Book (`std::vector`) architecture, this matching engine is hardcoded to support a maximum price of **$1000.00**. Prices are multiplied by 100 (cents) to map directly to array indices ($O(1)$ memory access). Orders above $1000.00 will throw an out-of-bounds error. This is a common trade-off in HFT to prioritize extreme latency reduction over unbounded price levels.

- **Flat Book Engine:** $O(1)$ limit order insertion using `std::vector` indexing (Cache-Aligned).
- **Object Pool & Intrusive Lists (Zero-Allocation):** Replaced `std::list` with a pre-allocated `std::vector<OrderNode>` Object Pool. Canceled orders are instantly recycled using an $O(1)$ Intrusive Free List, achieving strict zero heap allocations during live trading.
- **Hardware-Level Optimization:** Utilizes zero-copy `std::move` semantics and C++20 `[[likely]]` / `[[unlikely]]` branch prediction hints to process orders with extreme low latency.
- **In-Place Order Modification:** Features a `modifyOrder` function that accurately simulates exchange Queue Priority. Decreasing an order's quantity modifies the node in-place to preserve its queue position, while increasing the quantity drops the order to the back of the queue.
- **Avellaneda-Stoikov Market Maker:** Implements the classic 2008 quantitative model for optimal market making, skewing quotes based on inventory risk and market volatility.
- **Stoikov Micro-Price Bot:** Implements a modern 2018 variation that anchors to a Volume-Weighted Micro-Price, defending against toxic order flow while maintaining strict inventory limits.
- **Hawkes Process Trade Clustering:** The retail Noise Traders use a self-exciting Hawkes Process (instead of a static Poisson distribution) to simulate realistic market panics, flash crashes, and FOMO momentum bursts.
- **Multi-threaded Web Dashboard:** Uses `cpp-httplib` to embed an HTTP server directly into the C++ backend. Serves a real-time Vanilla JS/HTML dashboard with live Chart.js PnL plots without blocking the hyper-fast execution of the trading engine.
- **Interactive Simulation Controls:** Features granular Play/Pause functionality. The simulation starts paused by default, allowing you to independently toggle the Market Maker bots or the Noise Traders. When paused, bots dynamically pull their active orders from the Limit Order Book.
- **Python Analytics:** Includes data logging to CSV and Python visualization scripts (`matplotlib`) for offline strategy simulation.

## 🛠️ Technology Stack

- **Core Engine:** C++20
- **Testing & Benchmarks:** Google Test (gtest), Google Benchmark
- **Build System:** CMake
- **Server:** cpp-httplib (Header-only)
- **Frontend UI:** Vanilla HTML/CSS/JavaScript, Chart.js
- **Analytics:** Python 3, Pandas, Matplotlib

## 📁 Folder Structure

```text
.
├── src/
│   ├── engine/
│   │   ├── order.hpp          # Structs for Orders and Trades
│   │   ├── order_book.hpp     # Limit Order Book class definition
│   │   └── order_book.cpp     # O(1) Flat Book vector arrays & logic
│   ├── simulation/
│   │   ├── price_process.hpp  # Geometric Brownian Motion math
│   │   └── noise_trader.cpp   # Random retail flow generator
│   ├── bot/
│   │   ├── avellaneda_stoikov_bot.cpp   # Avellaneda-Stoikov 2008 bot
│   │   └── stoikov_microprice_bot.cpp  # Stoikov Micro-Price 2018 Bot
│   ├── httplib.h              # C++ Web Server library
│   └── main.cpp               # Multi-threading & CLI entry point
├── ui/
│   └── index.html             # Vanilla JS live dashboard
├── scripts/
│   └── plot_pnl.py            # Matplotlib performance charting
├── CMakeLists.txt             # C++ Build configuration
├── commands.md                # CLI commands guide
└── guide.md                   # Complete interview deep-dive
```

## 📖 Getting Started

Please refer to [commands.md](./commands.md) for a comprehensive list of commands on how to:
1. Compile the project (C++20).
2. Run the rigorous Google Test suite (`run_tests`).
3. Run the nanosecond Google Benchmarks (`run_benchmarks`).
4. Run the interactive CLI to place manual orders.
5. Run the offline Bot Simulation (`sim <steps> both`) and plot the head-to-head PnL graph.
6. Boot up the Live UI Dashboard!
