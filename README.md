# Limit Order Book & Avellaneda-Stoikov Simulator

A high-performance Limit Order Book (LOB) matching engine written in C++, complete with a stochastic market maker bot simulating real-world HFT liquidity provision, and a live web dashboard.

## 🚀 Key Features

- **$\mathcal{O}(1)$ Order Cancellation:** The core matching engine uses an industry-standard combination of Red-Black Trees (`map`), Doubly-Linked Lists (`list`), and Hash Maps (`unordered_map`) to guarantee constant-time average order cancellation.
- **Avellaneda-Stoikov Market Maker:** Implements the famous quantitative model for optimal market making. The bot dynamically adjusts its bid/ask spread and reservation price based on its current inventory risk and market volatility.
- **Strict Risk Management:** Features strict, dynamic purchasing power sizing to ensure cash limits (e.g. -$1,000) and inventory limits (±100 shares) are never breached mathematically.
- **GBM Noise Traders:** Simulates a realistic market environment by generating order flow using Geometric Brownian Motion (GBM).
- **Multi-threaded Web Dashboard:** Uses `cpp-httplib` to embed an HTTP server directly into the C++ backend. Serves a real-time Vanilla JS/HTML dashboard with live Chart.js PnL plots without blocking the hyper-fast execution of the trading engine.
- **Python Analytics:** Includes data logging to CSV and Python visualization scripts (`matplotlib`) for offline strategy backtesting.

## 🛠️ Technology Stack

- **Core Engine:** C++17
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
│   │   └── order_book.cpp     # O(1) matching & cancellation logic
│   ├── simulation/
│   │   ├── price_process.hpp  # Geometric Brownian Motion math
│   │   ├── noise_trader.hpp
│   │   └── noise_trader.cpp   # Random retail flow generator
│   ├── bot/
│   │   ├── market_maker.hpp
│   │   └── market_maker.cpp   # Avellaneda-Stoikov optimal quoting
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
1. Compile the project.
2. Run the interactive CLI to place manual orders.
3. Run the matching engine throughput benchmark.
4. Run the offline Bot Simulation and plot its PnL graph.
5. Boot up the Live UI Dashboard!
