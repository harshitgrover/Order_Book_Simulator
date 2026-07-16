#include "simulation/noise_trader.hpp"

using namespace std;

NoiseTrader::NoiseTrader(shared_ptr<OrderBook> book, double start_price, double rate)
    : order_book(book), 
      gbm(start_price, 0.0, 0.05, 1.0/86400.0),
      base_rate(rate), 
      next_order_id(1000000),
      generator(random_device{}()), 
      uniform_dist(0.0, 1.0) {}

double NoiseTrader::tick() {
    double current_rate = base_rate + buy_excitement + sell_excitement;
    exponential_distribution<double> exponential_dist(current_rate);
    double time_to_next = exponential_dist(generator);
    double true_price = gbm.nextPrice();
    
    double buy_prob = (base_rate/2.0 + buy_excitement) / current_rate;
    Side side = (uniform_dist(generator) < buy_prob) ? Side::BUY : Side::SELL;
    OrderType type = (uniform_dist(generator) < 0.2) ? OrderType::MARKET : OrderType::LIMIT;
    
    Order order;
    order.order_id = next_order_id++;
    order.side = side;
    order.type = type;
    order.quantity = 1 + static_cast<uint64_t>(uniform_dist(generator) * 100);
    order.timestamp = 0;
    
    if (type == OrderType::LIMIT) {
        double spread = 0.05;
        double random_offset = uniform_dist(generator) * 0.1;
        
        if (side == Side::BUY) {
            order.price = true_price - spread - random_offset;
        } else {
            order.price = true_price + spread + random_offset;
        }
        
        order.price = round(order.price * 100.0) / 100.0;
        order_book->addLimitOrder(order);
    } else {
        order.price = 0.0;
        order_book->addMarketOrder(order);
    }
    
    // Hawkes Excitement Jump
    if (use_hawkes) {
        if (side == Side::BUY) {
            buy_excitement += alpha;
        } else {
            sell_excitement += alpha;
        }
        
        // Decay the excitement over the time that just passed
        buy_excitement *= exp(-beta * time_to_next);
        sell_excitement *= exp(-beta * time_to_next);
    } else {
        buy_excitement = 0.0;
        sell_excitement = 0.0;
    }
    
    return time_to_next / 86400.0;
}
