# Order Book Simulator - Commands Guide

This document contains all the commands you need to build, test, and run the various components of the Order Book Simulator.

## 1. Build the Project
Whenever you change the C++ code, you must recompile the project:
```bash
cd build
make
```

## 2. Interactive CLI Mode
You can start the interactive command-line interface to place manual orders:
```bash
./build/simulator
```
**Available Commands inside CLI:**
- `buy <price> <qty>` - Place a limit buy order (e.g., `buy 100.5 10`)
- `sell <price> <qty>` - Place a limit sell order (e.g., `sell 101.0 10`)
- `market buy <qty>` - Place a market buy order
- `ioc sell <price> <qty>` - Place an Immediate-or-Cancel order
- `book` - Print the current order book to the console
- `quit` - Exit the CLI

## 3. Benchmark the Engine
Test the raw matching engine throughput (processes 1 million limit orders):
```bash
./build/simulator <<< "bench"
```

## 4. Run the Bot Simulation & Plot PnL (Phase 5)
Run the Avellaneda-Stoikov Market Maker against 5,000 noise trader events, logging data to a CSV. Then run the Python script to plot its inventory and PnL:
```bash
# 1. Run the simulation and generate sim_results.csv
./build/simulator <<< "sim 5000"

# 2. Activate the Python virtual environment and plot
source venv/bin/activate
python3 scripts/plot_pnl.py
```
*The resulting graph will be saved as `performance_plot.png` in the project root.*

## 5. Run the Live UI Dashboard (Phase 6)
Start the C++ HTTP server, which runs the simulation continuously in the background and serves a sleek web UI on port 8080:
```bash
./build/simulator <<< "serve"
```
Once it says it's running, open your web browser and go to:
**[http://localhost:8080/](http://localhost:8080/)**
