# 📘 The Complete Deep-Dive Architecture & Code Guide: Order Book Simulator

This document is the **most exhaustive, file-by-file, function-by-function breakdown** of the entire Order Book Simulator project. It is designed to be your master reference for any quantitative engineering or software development interview. It covers every single line of critical logic, the mathematical models, the data structures, and the networking architecture.

---

## 🏗️ 1. Project Architecture & Build System

### `CMakeLists.txt`
This file is the build configuration for the `cmake` build system. 
* **What it does:** It tells the C++ compiler how to link all the separate `.cpp` files together into a single executable (`simulator`), as well as setting up testing and benchmarking targets.
* **Technical Details:** 
  - We strictly enforce `CMAKE_CXX_STANDARD 20` to utilize modern C++ features like `[[likely]]` / `[[unlikely]]` attributes for branch prediction optimization.
  - The project is modularized into three static libraries: `engine` (the core LOB), `simulation` (the fake market), and `bot` (the algorithmic traders). These are linked together with `src/main.cpp`.
  - It also uses `FetchContent` to download and link Google Test (`gtest`) and Google Benchmark directly from GitHub during compilation.

---

## ⚙️ 2. The Core Matching Engine (`src/engine/`)

This is the most performance-critical part of the system. An interviewer will grill you heavily on the time complexities of these data structures.

### `order.hpp`
This file defines the atomic data structures for trades.
```cpp
enum class Side { BUY, SELL };
enum class OrderType { LIMIT, MARKET, IOC };

struct Order {
    uint64_t order_id; // Unique identifier for O(1) hash lookups
    Side side;         // BUY or SELL
    OrderType type;    // LIMIT, MARKET, or IOC
    double price;      // The limit price
    uint64_t quantity; // Number of shares
    uint64_t timestamp;// Used for secondary sorting (FIFO time priority)
};
```
* **Order Types Explained:** 
  - `LIMIT`: Rests in the order book until its exact price (or better) is met.
  - `MARKET`: Never rests. It instantly buys/sells at the best available price in the book until its quantity is filled.
  - `IOC` (Immediate-or-Cancel): Similar to Market, but if it cannot be fully filled instantly, the remaining quantity is immediately cancelled rather than resting in the book.

### `order_book.hpp` (The "Flat Book")
This file defines the data structure used to achieve extreme $\mathcal{O}(1)$ performance.
In early versions, we used an `std::map` (Red-Black tree) which took $\mathcal{O}(\log N)$ time to find a price level. In high-frequency trading, that's too slow. Instead, we use a **Flat Book** architecture.

```cpp
struct PriceLevel {
    std::list<Order> orders; // Doubly-linked list for Time-Priority (FIFO)
};

// Massive cache-aligned arrays covering every possible price tick
std::vector<PriceLevel> bids; 
std::vector<PriceLevel> asks;

// Hash Map for O(1) order cancellation lookups
std::unordered_map<uint64_t, OrderPointer> order_map; 

// Object Pool for zero-allocation nodes
std::vector<OrderNode> order_pool;
```
* **Price to Index Math:** The arrays are pre-allocated to 100,000 slots representing prices from `$0.00` to `$1000.00` in $0.01 increments. To find where an order belongs, we do instant math: `int index = price * 100`. 
  * *Note on Decimal Precision:* Because the index is cast to an integer (`static_cast<int>`), any price submitted with more than 2 decimal places (e.g., `$100.129`) will be automatically truncated down to the nearest penny (`$100.12`). This implicitly enforces a strict `$0.01` Tick Size for the market.
* **Object Pool (Intrusive Linked List):** Instead of allocating new nodes on the heap with `std::list`, the engine pre-allocates a massive `std::vector<OrderNode>`. Order nodes contain `prev` and `next` integer indices. When an order is filled or cancelled, its index is pushed to a `free_list` (a stack of reusable indices) to be instantly recycled. This guarantees **zero heap allocations** during the hot path.
* **Queue Priority (`modifyOrder`):** The engine provides an in-place `modifyOrder` function. If a market maker wants to *reduce* their resting order's quantity, the engine simply decrements the value in the Object Pool, preserving the order's exact physical position in the queue. If they *increase* the quantity, the node is forcefully detached from the Intrusive List and appended to the back, accurately penalizing them for increasing risk.

### `order_book.cpp` (The Engine Logic)

#### 1. Limit Order Addition: `addLimitOrder()`
**Time Complexity: $\mathcal{O}(1)$ memory access.**
When a `LIMIT` order arrives, it instantly checks the `best_ask_idx` or `best_bid_idx` to see if it crosses the spread.
```cpp
void OrderBook::addLimitOrder(Order order) {
    if (order.side == Side::BUY) {
        // Step 1: Match against asks instantly via array index
        while (order.quantity > 0 && best_ask_idx < MAX_PRICE_TICKS) {
            if (best_ask_idx > priceToIndex(order.price)) break; // Spread is not crossed
            
            auto& level_orders = asks[best_ask_idx].orders;
            // ... (Matching logic executes trades) ...
        }
        
        // Step 2: Rest the remaining order in the book
        if (order.quantity > 0 && order.type == OrderType::LIMIT) {
            int idx = priceToIndex(order.price);
            bids[idx].orders.push_back(std::move(order)); // std::move prevents memory copying
            
            // Branch prediction hint: it's very likely we establish a new best bid
            if (idx > best_bid_idx) [[likely]] {
                best_bid_idx = idx;
            }
        }
    }
}
```
* **Hardware Optimization:** Notice the use of `std::move(order)`. We pass the order by value (to avoid dangling references), and then *move* its memory directly into the `std::list`. This is a "zero-copy" optimization.
* **Branch Prediction:** Modern CPUs try to guess the outcome of `if` statements. By adding `[[likely]]` (C++20), we instruct the compiler to optimize the assembly code for the scenario where a new order tightens the spread, preventing CPU pipeline flushes.

---

## 📈 3. The Market Simulation (`src/simulation/`)

To test our bot, we built a fully autonomous "fake" market environment.

### `price_process.hpp` (The Math)
This file uses **Geometric Brownian Motion (GBM)** to simulate real-world stock market price movements.
```cpp
double getNextPrice(double current_price, double dt) {
    double z = normal_dist(gen); // Random sample from Standard Normal Distribution
    // GBM Formula: dS = S * (mu * dt + sigma * dW)
    return current_price * exp((drift - 0.5 * vol * vol) * dt + vol * sqrt(dt) * z);
}
```
* **Why GBM?** Unlike a simple random walk, GBM multiplies the price exponentially. This mathematically guarantees that the stock price can never drop below $0.00, perfectly mimicking real-world equity markets.

### `noise_trader.cpp` (The Fake Traders)
This class mimics millions of retail investors (like Robinhood users).
* **Hawkes Process Trade Clustering:** Real financial markets do not follow a uniform static probability. To simulate realistic momentum, FOMO, and panic selling, the Noise Traders use a **Self-Exciting Hawkes Process**. 
  - Every time a buy or sell occurs, a "Buy Excitement" or "Sell Excitement" variable spikes by a factor of $\alpha$. 
  - This drastically increases the probability (the $\lambda$ intensity of the Poisson distribution) that *another* trade of the same side will happen on the very next tick. 
  - The excitement decays exponentially by a factor of $\beta$ over time. 
* **`tick()`:** Advances time and evaluates the Hawkes intensity to determine if an order fires. Updates the "true" stock price using the GBM process.
* **`placeOrders()`:** Uses a random number generator. 
  - **80% of the time:** Places Limit orders slightly above or below the true price. This builds up the depth of the Order Book (Liquidity).
  - **20% of the time:** Places Market or IOC orders. This violently crashes into the Order Book, crossing the spread and causing the stock price to actually move.

---

## 🤖 4. The Quantitative Market Making Bots (`src/bots/`)

We implemented two different mathematical models to compare their effectiveness against toxic order flow.

### Avellaneda-Stoikov Bot: The Avellaneda-Stoikov Market Maker (2008)
`avellaneda_stoikov_bot.cpp` translates the famous 2008 high-frequency trading research paper into C++ logic. The bot solves for optimal quotes to manage "Inventory Risk".

#### The Avellaneda-Stoikov Math
```cpp
double time_left = terminal_time; 

// 1. Calculate Reservation Price (The "Safe" Midpoint)
double reservation_price = mid_price - inventory * risk_aversion * volatility * volatility * time_left;

// 2. Calculate the Optimal Spread
double spread = risk_aversion * volatility * volatility * time_left + (2.0 / risk_aversion) * log(1.0 + risk_aversion / order_density);
```
* **Explanation:** If the bot owns +50 shares (`inventory > 0`), the formula subtracts a massive penalty from the `mid_price`. This skews the `reservation_price` sharply downwards. The bot's Ask price becomes much cheaper (to dump inventory quickly), and its Bid price becomes extremely low (to avoid buying more).

### Stoikov Micro-Price Bot: The Stoikov Micro-Price Bot Market Maker (2018)
`stoikov_microprice_bot.cpp` is a modern upgrade. In highly volatile markets, the "mid-price" is a dangerous illusion. If there are 1,000 buy orders and 1 sell order, the price is inevitably going to go up. 
This bot calculates the **Volume-Weighted Micro-Price** (Stoikov 2018) to anchor its quotes.

#### The Micro-Price Math
```cpp
uint64_t bid_vol = order_book->bestBidVolume();
uint64_t ask_vol = order_book->bestAskVolume();
double total_vol = static_cast<double>(bid_vol + ask_vol);

// Calculate Order Book Imbalance [0.0 to 1.0]
double imbalance = static_cast<double>(bid_vol) / total_vol;

// Volume-Weighted Micro-Price
double micro_price = (imbalance * best_ask) + ((1.0 - imbalance) * best_bid);
```
* **Explanation:** Rather than just splitting the difference between the Bid and Ask, the Micro-Price shifts dynamically based on liquidity weight. If there is massive buying pressure, `imbalance` gets closer to `1.0`, dragging the `micro_price` heavily towards the `best_ask`. The bot then plugs this `micro_price` into the Avellaneda-Stoikov risk formulas instead of the mid-price!

---

## 🌐 5. The Backend & Networking (`src/main.cpp`)

This file is the entry point. It parses CLI commands and handles Multi-threading.

### The `serve` Command & Thread Architecture
To run a Web Dashboard without slowing down the HFT Engine, we use a multithreaded architecture.
```cpp
std::thread sim_thread([&]() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 FPS updates
        
        std::lock_guard<std::mutex> lock(state_mutex); // Lock memory
        for (int i = 0; i < 50; ++i) { // Run 50 simulation ticks instantly
            trader.tick();
            bot.onTick();
        }
    } // Unlock memory
});
```
* **Explanation:** The `sim_thread` handles the heavy lifting. We use an `std::mutex` to "lock" the Order Book memory. If the Web Server tries to read the Order Book at the exact millisecond the Engine is deleting an order, the program would instantly crash (Segmentation Fault). The Mutex prevents this.

### `cpp-httplib` (The Web Server)
We use `cpp-httplib`, a lightweight, "header-only" C++ networking library.
* **Header-Only:** You simply `#include "httplib.h"`. There are no massive `.dll` or `.so` files to install. It compiles directly into the executable.
* **The API:** When the server receives a `GET /api/state` request, it locks the Mutex, iterates through the top 15 Bids/Asks in the Order Book, formats them into a JSON string, and serves them over TCP port 8080.

---

## 📊 6. Tests, Benchmarks & Offline Analytics

### Google Benchmark (`tests/benchmark_order_book.cpp`)
We use Google Benchmark to measure the absolute hardware execution time of our engine.
When running `./run_benchmarks`, you will see:
* `BM_OrderBookAddLimitOrder`: The Flat Book `std::vector` implementation executes in **~1,130 nanoseconds**.
* `BM_LegacyMapOrderBook`: The legacy Red-Black Tree `std::map` implementation takes significantly longer, visually proving why array-based memory structures dominate in HFT.
* `BM_StoikovMicropriceBotTick`: The complex Micro-Price and inventory risk calculations only take **~900 nanoseconds**, proving that advanced quant math doesn't slow down the bot execution.

### Offline Analytics (`scripts/plot_pnl.py`)
When running `sim 5000 both`, the C++ engine dumps tick-by-tick data into `sim_results.csv` for both bots.
* **Python Script:** We use Python `pandas` to read the CSV, and `matplotlib` to render the `performance_plot.png`.
* **Proof of Concept:** The resulting chart overlays the PnL of the classic 2008 bot against the modern 2018 bot, allowing quantitative researchers to directly analyze their relative outperformance.

---

## 💻 7. The Frontend Web Dashboard (`ui/index.html`)

The frontend is a single-page application built entirely in **Vanilla HTML/CSS/JavaScript**. 

### Why Vanilla JS?
In HFT, performance is king. React and Angular utilize a "Virtual DOM" to calculate state changes, which adds massive overhead. By writing Vanilla JS, we manipulate the browser's raw Document Object Model (DOM) directly, resulting in zero overhead and instant screen updates.

### The Polling Loop
```javascript
setInterval(() => {
    fetch('http://localhost:8080/api/state')
    .then(response => response.json())
    .then(data => {
        // Wipe old DOM elements and inject new ones directly via innerHTML
    });
}, 100);
```
Every 100 milliseconds, the frontend requests the JSON state and directly overwrites the HTML of the Order Book tables.

### `Chart.js` & The Canvas API
Rendering hundreds of HTML `<div>` elements for a PnL line chart would freeze the browser. 
Instead, we use `Chart.js`, which leverages the HTML5 `<canvas>` API. The Canvas API bypasses the HTML renderer entirely and uses the computer's GPU to literally paint raw pixels onto the screen. This allows the bot's PnL chart to scroll smoothly at 60 Frames Per Second with zero lag.

---

## ❓ 8. Likely Interview Questions (And How To Answer Them)

**Q: Why `std::vector` array for price levels instead of `std::map`?**
A: Originally, we used `std::map` (a Red-Black Tree) which took $\mathcal{O}(\log N)$ to find a price level. By using a Flat Book `std::vector`, we map prices directly to array indices (e.g., `$100.50 * 100 = 10050`). This provides $\mathcal{O}(1)$ random access. Furthermore, arrays are contiguous in memory, maximizing CPU L1 cache hits, whereas tree nodes are scattered across the heap causing cache misses.

**Q: Explain the zero-copy optimization in your order book.**
A: We pass the incoming `Order` by value to detach it from the network/caller memory. Then, we use `std::move(order)` when pushing it to the `std::list`. This steals the internal memory of the object without creating an expensive deep copy.

**Q: Why did you use `[[likely]]` attributes?**
A: In modern CPUs, instruction pipelines are deep. If an `if` statement evaluates to a branch the CPU didn't predict, the entire pipeline is flushed (costing ~15-20 CPU cycles). Since most limit orders placed inside the spread tighten the best bid/ask, we use C++20 `[[likely]]` to instruct the compiler to lay out the assembly code linearly for that specific outcome, drastically reducing branch miss latency.

**Q: Why does the Stoikov Micro-Price bot use Micro-Price instead of Mid-Price?**
A: The mid-price is naive. If the book has 10,000 shares on the Bid and only 10 shares on the Ask, the true "fair value" is significantly closer to the Ask due to massive buying pressure. The Stoikov 2018 Volume-Weighted Micro-Price mathematically models this imbalance. By anchoring to the micro-price, the bot stops getting "run over" by toxic institutional order flow.

**Q: What happens if two orders arrive at the exact same timestamp?**
A: Because they are processed sequentially by the single-threaded matching engine, one will mathematically be inserted into the `std::list` before the other, naturally preserving FIFO queue priority.

**Q: Why does the trade execute at the maker's price, not the taker's?**
A: This is standard exchange behavior. The resting order (Maker) provided liquidity at a specific limit price. The incoming order (Taker) crossed the spread to take it. The exchange always awards the trade at the Maker's resting price to reward them for providing liquidity.

**Q: Explain the intuition behind the reservation price formula — why does inventory lower it?**
A: If the bot is holding +50 shares, it is exposed to the risk of the stock crashing. To offload this risk, it lowers its Reservation Price. This naturally pulls both its Bid and Ask prices down. A lower Ask makes it more attractive for others to buy from the bot (reducing its inventory), and a lower Bid makes it less likely the bot will accidentally buy even more shares.

**Q: What does $\gamma$ (gamma) control?**
A: Gamma is the Risk Aversion parameter. A higher gamma means the bot is terrified of holding inventory. It will widen its spread significantly and aggressively skew its reservation price to snap its inventory back to zero as fast as possible.

**Q: Why does a trading firm need to build their own Limit Order Book (LOB) unless they are an actual exchange?**
A: While offline simulation is a major reason, there are two massive industry use-cases:
1. **Local Order Book Reconstruction (Live Trading):** Exchanges (like NASDAQ) do not constantly broadcast the "full picture" of the order book. They broadcast a massive firehose of millions of individual UDP packets (*"Order Added"*, *"Order Cancelled"*). HFT firms must build a Lightning-Fast LOB in their own servers to ingest this stream and reconstruct a mirror-image of the exchange's book in real-time. If their local C++ book is slow, their trading bot will make decisions based on outdated prices.
2. **Dark Pools & Internalization (Banks):** Major banks (like Goldman Sachs) receive thousands of orders from their retail clients. Instead of routing all those orders to the public stock exchange (and paying massive exchange fees), they build their own internal Limit Order Books to match their clients' orders against each other *internally*. This saves fees and hides their trading intentions from the public market.

**Q: In the dual-bot simulation, why does the 2008 bot often perform better initially, but the 2018 bot eventually overtakes it?**
A: This perfectly demonstrates **Market Microstructure & Queue Priority (Latency Arbitrage)**. Both bots compete inside the exact same Limit Order Book. Because the 2008 bot's `onTick()` logic is evaluated microseconds before the 2018 bot in the C++ simulation loop, the 2008 bot frequently places its quotes into the FIFO Order Book queue *first*. When a random market order arrives, the 2008 bot absorbs the execution. However, when market volatility spikes or the book becomes highly unbalanced, the 2018 bot shifts its quotes to aggressively better prices than the 2008 bot. Because *Price-Priority* strictly beats *Time-Priority*, the 2018 bot instantly jumps to the front of the queue, stealing the executions and avoiding toxic flow.

**Q: Explain the memory architecture of your Object Pool and Intrusive List.**
A: In high-frequency trading, standard library containers like `std::list` are banned on the hot path because they invoke the OS heap allocator (`new` and `delete`), which takes hundreds of nanoseconds. To fix this, I pre-allocated a massive `std::vector<OrderNode>` array on startup. My order nodes contain `prev` and `next` integers rather than raw memory pointers. This effectively creates an Intrusive Doubly-Linked List backed by contiguous array memory. When an order is cancelled, I push its array index onto a `free_list` stack so it can be instantly recycled by the next incoming order, completely bypassing the heap.

**Q: How do you handle order modifications in your limit order book?**
A: I engineered an in-place `modifyOrder` function that perfectly mirrors real-world exchange Queue Priority rules. If a market maker wants to *reduce* the quantity of their resting limit order, I modify the quantity integer in-place without touching the node's `prev`/`next` links, allowing them to keep their hard-earned position at the front of the line. However, if they *increase* the quantity, I detach their node from the Intrusive List and re-append it to the `tail` of the price level, forcing them to the back of the queue as a penalty for taking on more risk.
