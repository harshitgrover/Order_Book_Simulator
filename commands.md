# Order Book Simulator - Commands Guide

This document contains all the commands you need to build, test, and run the various components of the Order Book Simulator.

## 1. Build the Project
Whenever you change the C++ code, you must recompile the project:
```bash
mkdir -p build && cd build
cmake ..
make -j4
```

## 2. Interactive CLI Mode
You can start the interactive command-line interface to place manual orders:
```bash
./build/simulator
```
**Available Commands inside CLI:**
```text
buy <price> <qty>         # Place a limit buy order (e.g., buy 100.5 10)
sell <price> <qty>        # Place a limit sell order (e.g., sell 101.0 10)
market buy <qty>          # Place a market buy order (sweeps the book)
market sell <qty>         # Place a market sell order
ioc buy <price> <qty>     # Place an Immediate-or-Cancel buy order
ioc sell <price> <qty>    # Place an Immediate-or-Cancel sell order
modify <order_id> <qty>   # Modify the quantity of an existing limit order
cancel <order_id>         # Cancel an existing limit order by its ID
book                      # Print the current limit order book to the console
bench                     # Run an internal performance benchmark (processes 1 million limit orders)
sim <steps> [-plot] [bot] # Run offline sim (e.g. `sim 5000 -plot`, defaults to `both`)
quit                      # Exit the CLI
```

## 3. Run Tests & Benchmarks
Test the raw matching engine throughput and bot latency (processes at sub-microsecond speeds):
```bash
# Run unit tests
./build/run_tests

# Run benchmarks (prints to terminal)
./build/run_benchmarks

# Run benchmarks and save the output to a text file
./build/run_benchmarks > benchmark_results.txt
```

## 4. Run the Bot Simulation & Plot PnL (Phase 5)
Run the Market Makers against noise trader events, logging data to a CSV. You can choose to run the classic Avellaneda-Stoikov bot, the modern Imbalance-Aware bot, or both simultaneously!

```bash
# Option A: Run BOTH bots side-by-side and automatically plot the results
./build/simulator sim 5000 -plot

# Option B: Run ONLY the Avellaneda-Stoikov bot
./build/simulator sim 5000 avellaneda -plot

# Option C: Run ONLY the Imbalance-Aware bot
./build/simulator sim 5000 imbalance -plot
```
*The resulting graph will be saved as `performance_plot.png` in the project root.*

## 5. Run the Live UI Dashboard (Phase 6)
Start the C++ HTTP server, which runs the simulation continuously in the background and serves a sleek web UI on port 8080:

```bash
# Option A: Start the UI with BOTH bots running (Default)
./build/simulator serve

# Option B: Start the UI with ONLY Avellaneda-Stoikov
./build/simulator serve 8080 bot1

# Option C: Start the UI with ONLY Imbalance-Aware
./build/simulator serve 8080 bot2
```

Once it says it's running, open your web browser and go to:
**[http://localhost:8080/](http://localhost:8080/)**

### Interactive UI Features
- **Granular Pausing:** The simulation starts paused by default. You can independently Start/Pause **Bot 1**, **Bot 2**, or the **GBM Market (Noise Traders)**.
- **Order Cancellation:** When a bot is paused, it actively pulls all of its open quotes from the limit order book, allowing you to observe how the market reacts.
- **Live State Viewing:** Toggle between Bot 1, Bot 2, or Both in the UI to view active Bid/Ask levels, PnL charts, and inventory without cluttering the screen.

### Stopping the UI Server
If you are running the UI server in your terminal, you can stop it by pressing `Ctrl + C`. 
If you have started it in the background or multiple instances are somehow running, you can kill all running instances by running:
```bash
killall simulator
```
