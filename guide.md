# 📘 The Complete Deep-Dive Architecture & Code Guide: Order Book Simulator

This document is the **most exhaustive, file-by-file, function-by-function breakdown** of the entire Order Book Simulator project. It is designed to be your master reference for any quantitative engineering or software development interview. It covers every single line of critical logic, the mathematical models, the data structures, and the networking architecture.

---

## 🏗️ 1. Project Architecture & Build System

### `CMakeLists.txt`
This file is the build configuration for the `cmake` build system. 
* **What it does:** It tells the C++ compiler how to link all the separate `.cpp` files together into a single executable (`simulator`).
* **Technical Details:** 
  - We strictly enforce `CMAKE_CXX_STANDARD 17` to utilize modern C++ features like `std::unordered_map` optimizations and multithreading primitives.
  - The project is modularized into three static libraries: `engine` (the core LOB), `simulation` (the fake market), and `bot` (the algorithmic trader). These are then linked together with `src/main.cpp` to create the final `simulator` executable.

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

### `order_book.hpp`
This file defines the hybrid data structure used to achieve optimal Time Complexities.

```cpp
struct PriceLevel {
    std::list<Order> orders; // Doubly-linked list for Time-Priority (FIFO)
};

// Red-Black Trees to keep price levels perfectly sorted at all times.
std::map<double, PriceLevel, std::greater<double>> bids; // Highest price first
std::map<double, PriceLevel, std::less<double>> asks;    // Lowest price first

struct OrderPointer {
    double price;
    std::list<Order>::iterator list_it;
};
// Hash Map for O(1) order location lookups
std::unordered_map<uint64_t, OrderPointer> order_map; 
```

### `order_book.cpp` (The Engine Logic)

#### 1. Order Cancellation: `cancelOrder()`
**Time Complexity: $\mathcal{O}(1)$ average.**
This is the "flex" of the project. If a trader wants to cancel Order #50, searching a tree takes $\mathcal{O}(\log N)$, and searching a list takes $\mathcal{O}(N)$. We bypass both.
```cpp
void OrderBook::cancelOrder(uint64_t order_id) {
    auto it = order_map.find(order_id); // 1. Hash lookup: O(1) time
    if (it == order_map.end()) return;  
    
    double price = it->second.price;
    auto list_it = it->second.list_it;  // 2. Retrieve exact memory pointer
    
    if (list_it->side == Side::BUY) {
        bids[price].orders.erase(list_it); // 3. Delete from Linked List: O(1) time!
        if (bids[price].orders.empty()) bids.erase(price); // Clean up empty price levels
    }
    order_map.erase(it); // 4. Remove from Hash Map
}
```

#### 2. Limit Order Addition: `addLimitOrder()`
**Time Complexity: $\mathcal{O}(\log N)$ to find price level, $\mathcal{O}(1)$ to insert.**
When a `LIMIT` order arrives, it first tries to match against the opposite side of the book (crossing the spread).
```cpp
void OrderBook::addLimitOrder(Order& order) {
    if (order.side == Side::BUY) {
        // Step 1: Match against asks (if our Bid >= Lowest Ask)
        while (order.quantity > 0 && !asks.empty()) {
            auto best_ask = asks.begin();
            if (best_ask->first > order.price) break; // Spread is not crossed
            
            // ... (Matching logic executes trades and reduces quantities) ...
        }
        
        // Step 2: If quantity remains, rest the order in the book
        if (order.quantity > 0 && order.type == OrderType::LIMIT) {
            bids[order.price].orders.push_back(order); // Push to back of FIFO list
            auto it = --bids[order.price].orders.end(); // Get iterator to the new order
            order_map[order.order_id] = {order.price, it}; // Store iterator in Hash Map
        }
    }
}
```

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
* **`tick()`:** Advances time by 0.01 seconds (100Hz) and updates the "true" stock price using the GBM process.
* **`placeOrders()`:** Uses a random number generator. 
  - **70% of the time:** Places Limit orders slightly above or below the true price. This builds up the depth of the Order Book (Liquidity).
  - **30% of the time:** Places Market or IOC orders. This violently crashes into the Order Book, crossing the spread and causing the stock price to actually move.

---

## 🤖 4. The Avellaneda-Stoikov Market Maker (`src/bot/`)

This is the Quantitative Finance masterpiece of the project. It translates a 2008 high-frequency trading research paper into C++ logic.

### The Objective
Market Makers profit by buying at the Bid and selling at the Ask, pocketing the "spread". However, if they accumulate too many shares (Inventory Risk) and the market crashes, they lose everything. The Avellaneda-Stoikov model mathematically solves for the optimal quotes to manage this risk.

### `market_maker.cpp` (`onTick` function)

#### 1. Inventory Syncing
```cpp
void MarketMaker::updateInventory() {
    Order order;
    // If our active_bid_id is NO LONGER in the order book, it means a Noise Trader 
    // bought it! We instantly increase our inventory and decrease our cash.
    if (active_bid_id != 0 && !order_book->getOrder(active_bid_id, order)) {
        inventory += active_bid_qty;
        cash -= (active_bid_price * active_bid_qty);
        active_bid_id = 0; // Reset
    }
}
```

#### 2. The Avellaneda-Stoikov Math
```cpp
double time_left = terminal_time; // Infinite-horizon constant lookahead

// 1. Calculate Reservation Price (The "Safe" Midpoint)
double reservation_price = mid_price - inventory * risk_aversion * volatility * volatility * time_left;

// 2. Calculate the Optimal Spread
double spread = risk_aversion * volatility * volatility * time_left + (2.0 / risk_aversion) * log(1.0 + risk_aversion / order_density);
```
* **Explanation:** If the bot owns +50 shares (`inventory > 0`), the formula subtracts a massive penalty from the `mid_price`. This skews the `reservation_price` sharply downwards. The bot's Ask price becomes much cheaper (to dump inventory quickly), and its Bid price becomes extremely low (to avoid buying more).

#### 3. Strict Risk Management (Hard Limits)
Even with advanced math, HFT firms use hard limits to prevent catastrophic bankruptcy.
```cpp
int max_buy_qty = std::min(10, 100 - inventory); // Never exceed 100 shares
if (cash < -1000.0) max_buy_qty = 0;             // Never drop below -$1,000 cash

if (max_buy_qty > 0) {
    // Place the bid...
} else {
    active_bid_id = 0; // Refuse to bid!
}
```

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

## 📊 6. The Offline Analytics (`scripts/plot_pnl.py`)

When running the `sim` command, the C++ engine runs as fast as the CPU allows (offline) and dumps tick-by-tick data into `sim_results.csv`.
* **Python Script:** We use Python `pandas` to read the CSV, and `matplotlib` to render a 2-panel chart. The top panel shows Mark-to-Market PnL, and the bottom shows Inventory. 
* **Proof of Concept:** The resulting chart mathematically proves that the Avellaneda-Stoikov model works—the inventory constantly snaps back to $0$, capturing the spread and generating a massive upward PnL trend.

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

**Q: Why `std::map` instead of a hashmap for price levels?**
A: A Limit Order Book must always know the *best* price instantly. `std::map` is a Red-Black Tree that keeps price levels sorted automatically, so we can just grab `bids.begin()` or `asks.begin()`. A hashmap doesn't preserve order, so we'd have to sort it manually every tick, which is slow.

**Q: Why is cancellation $\mathcal{O}(1)$? Walk me through it.**
A: When an order is added, we push it to the back of an `std::list` (for time priority) and save that exact `std::list::iterator` inside an `std::unordered_map` keyed by `order_id`. When a cancel request comes in, we hash lookup the ID in $\mathcal{O}(1)$ to get the iterator, and call `list.erase(iterator)` which is also $\mathcal{O}(1)$ since it just rewires two pointers.

**Q: What happens if two orders arrive at the exact same timestamp?**
A: Because they are processed sequentially by the single-threaded matching engine, one will mathematically be inserted into the `std::list` before the other, naturally preserving FIFO queue priority.

**Q: Why does the trade execute at the maker's price, not the taker's?**
A: This is standard exchange behavior. The resting order (Maker) provided liquidity at a specific limit price. The incoming order (Taker) crossed the spread to take it. The exchange always awards the trade at the Maker's resting price to reward them for providing liquidity.

**Q: Explain the intuition behind the reservation price formula — why does inventory lower it?**
A: If the bot is holding +50 shares, it is exposed to the risk of the stock crashing. To offload this risk, it lowers its Reservation Price. This naturally pulls both its Bid and Ask prices down. A lower Ask makes it more attractive for others to buy from the bot (reducing its inventory), and a lower Bid makes it less likely the bot will accidentally buy even more shares.

**Q: What does $\gamma$ (gamma) control?**
A: Gamma is the Risk Aversion parameter. A higher gamma means the bot is terrified of holding inventory. It will widen its spread significantly and aggressively skew its reservation price to snap its inventory back to zero as fast as possible.

**Q: Why does a trading firm need to build their own Limit Order Book (LOB) unless they are an actual exchange?**
A: While backtesting and simulation is a major reason, there are two massive industry use-cases:
1. **Local Order Book Reconstruction (Live Trading):** Exchanges (like NASDAQ) do not constantly broadcast the "full picture" of the order book. They broadcast a massive firehose of millions of individual UDP packets (*"Order Added"*, *"Order Cancelled"*). HFT firms must build a Lightning-Fast LOB in their own servers to ingest this stream and reconstruct a mirror-image of the exchange's book in real-time. If their local C++ book is slow, their trading bot will make decisions based on outdated prices.
2. **Dark Pools & Internalization (Banks):** Major banks (like Goldman Sachs) receive thousands of orders from their retail clients. Instead of routing all those orders to the public stock exchange (and paying massive exchange fees), they build their own internal Limit Order Books to match their clients' orders against each other *internally*. This saves fees and hides their trading intentions from the public market.
