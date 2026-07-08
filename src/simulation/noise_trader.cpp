#include "simulation/noise_trader.hpp"

using namespace std;

NoiseTrader::NoiseTrader(shared_ptr<OrderBook> book, double start_price, double rate)
    : order_book(book), 
      gbm(start_price, 0.0, 0.05, 1.0/86400.0),
      arrival_rate(rate), 
      next_order_id(1000000),
      generator(random_device{}()), 
      exponential_dist(rate), 
      uniform_dist(0.0, 1.0) {}

double NoiseTrader::tick() {
    double time_to_next = exponential_dist(generator);
    double true_price = gbm.nextPrice();
    
    Side side = (uniform_dist(generator) > 0.5) ? Side::BUY : Side::SELL;
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
    
    return time_to_next / 86400.0;
}
