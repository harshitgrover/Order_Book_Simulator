#include "bot/imbalance_bot.hpp"
#include <cmath>

ImbalanceMarketMaker::ImbalanceMarketMaker(shared_ptr<OrderBook> book, double gamma, double sigma, double k, double T)
    : order_book(book), inventory(0), cash(0.0), next_order_id(3000000), // Different starting ID so they don't clash
      active_bid_id(0), active_bid_qty(0), active_bid_price(0.0),
      active_ask_id(0), active_ask_qty(0), active_ask_price(0.0),
      risk_aversion(gamma), volatility(sigma), order_density(k), 
      terminal_time(T), current_time(0.0), last_micro_price(100.0) {}

void ImbalanceMarketMaker::updateInventory() {
    Order order;
    
    if (active_bid_id != 0) {
        if (order_book->getOrder(active_bid_id, order)) {
            uint64_t filled = active_bid_qty - order.quantity;
            if (filled > 0) {
                inventory += filled;
                cash -= filled * active_bid_price;
                active_bid_qty = order.quantity;
            }
        } else {
            inventory += active_bid_qty;
            cash -= active_bid_qty * active_bid_price;
            active_bid_id = 0;
            active_bid_qty = 0;
        }
    }
    
    if (active_ask_id != 0) {
        if (order_book->getOrder(active_ask_id, order)) {
            uint64_t filled = active_ask_qty - order.quantity;
            if (filled > 0) {
                inventory -= filled;
                cash += filled * active_ask_price;
                active_ask_qty = order.quantity;
            }
        } else {
            inventory -= active_ask_qty;
            cash += active_ask_qty * active_ask_price;
            active_ask_id = 0;
            active_ask_qty = 0;
        }
    }
}

void ImbalanceMarketMaker::onTick(double dt) {
    current_time += dt;
    
    updateInventory();
    
    double best_bid = order_book->bestBid();
    double best_ask = order_book->bestAsk();
    
    if (best_bid == 0.0 || best_ask == 0.0) return;
    
    uint64_t bid_vol = order_book->bestBidVolume();
    uint64_t ask_vol = order_book->bestAskVolume();
    
    double micro_price = order_book->midPrice();
    if (bid_vol + ask_vol > 0) {
        micro_price = (best_bid * ask_vol + best_ask * bid_vol) / static_cast<double>(bid_vol + ask_vol);
    }
    
    last_micro_price = micro_price;
    
    double time_left = terminal_time; 
    
    // Use micro_price instead of mid_price for reservation price
    double reservation_price = micro_price - inventory * risk_aversion * volatility * volatility * time_left;
    double spread = risk_aversion * volatility * volatility * time_left + (2.0 / risk_aversion) * log(1.0 + risk_aversion / order_density);
    
    double bid_quote = reservation_price - spread / 2.0;
    double ask_quote = reservation_price + spread / 2.0;
    
    bid_quote = round(bid_quote * 100.0) / 100.0;
    ask_quote = round(ask_quote * 100.0) / 100.0;
    
    

    uint64_t desired_qty = 10;
    
    uint64_t max_buy_qty = desired_qty;
    double available_cash = cash + 1000.0;
    if (available_cash <= 0.0 || bid_quote <= 0.0) {
        max_buy_qty = 0;
    } else {
        uint64_t affordable_qty = static_cast<uint64_t>(floor(available_cash / bid_quote));
        if (affordable_qty < max_buy_qty) max_buy_qty = affordable_qty;
    }
    
    if (inventory < 100) {
        uint64_t room_to_buy = static_cast<uint64_t>(100 - inventory);
        if (room_to_buy < max_buy_qty) max_buy_qty = room_to_buy;
    } else {
        max_buy_qty = 0;
    }
    
    uint64_t max_sell_qty = desired_qty;
    if (inventory > -100) {
        uint64_t room_to_sell = static_cast<uint64_t>(inventory + 100);
        if (room_to_sell < max_sell_qty) max_sell_qty = room_to_sell;
    } else {
        max_sell_qty = 0;
    }
    
    if (active_bid_id == 0) {
        if (max_buy_qty > 0) {
            Order bid_order{++next_order_id, Side::BUY, OrderType::LIMIT, bid_quote, max_buy_qty, 0};
            order_book->addLimitOrder(bid_order);
            active_bid_id = bid_order.order_id;
            active_bid_price = bid_quote;
            active_bid_qty = max_buy_qty;
        }
    } else {
        if (max_buy_qty == 0) {
            order_book->cancelOrder(active_bid_id);
            active_bid_id = 0;
        } else if (active_bid_price != bid_quote) {
            order_book->cancelOrder(active_bid_id);
            Order bid_order{++next_order_id, Side::BUY, OrderType::LIMIT, bid_quote, max_buy_qty, 0};
            order_book->addLimitOrder(bid_order);
            active_bid_id = bid_order.order_id;
            active_bid_price = bid_quote;
            active_bid_qty = max_buy_qty;
        } else if (active_bid_qty != max_buy_qty) {
            order_book->modifyOrder(active_bid_id, max_buy_qty);
            active_bid_qty = max_buy_qty;
        }
    }

    if (active_ask_id == 0) {
        if (max_sell_qty > 0) {
            Order ask_order{++next_order_id, Side::SELL, OrderType::LIMIT, ask_quote, max_sell_qty, 0};
            order_book->addLimitOrder(ask_order);
            active_ask_id = ask_order.order_id;
            active_ask_price = ask_quote;
            active_ask_qty = max_sell_qty;
        }
    } else {
        if (max_sell_qty == 0) {
            order_book->cancelOrder(active_ask_id);
            active_ask_id = 0;
        } else if (active_ask_price != ask_quote) {
            order_book->cancelOrder(active_ask_id);
            Order ask_order{++next_order_id, Side::SELL, OrderType::LIMIT, ask_quote, max_sell_qty, 0};
            order_book->addLimitOrder(ask_order);
            active_ask_id = ask_order.order_id;
            active_ask_price = ask_quote;
            active_ask_qty = max_sell_qty;
        } else if (active_ask_qty != max_sell_qty) {
            order_book->modifyOrder(active_ask_id, max_sell_qty);
            active_ask_qty = max_sell_qty;
        }
    }
}

void ImbalanceMarketMaker::cancelAll() {
    if (active_bid_id != 0) {
        order_book->cancelOrder(active_bid_id);
        active_bid_id = 0;
        active_bid_qty = 0;
    }
    if (active_ask_id != 0) {
        order_book->cancelOrder(active_ask_id);
        active_ask_id = 0;
        active_ask_qty = 0;
    }
}
