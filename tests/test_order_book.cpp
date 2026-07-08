#include <gtest/gtest.h>
#include "engine/order_book.hpp"
#include "engine/order.hpp"

// Test if the OrderBook initializes correctly
TEST(OrderBookTest, Initialization) {
    OrderBook book;
    EXPECT_EQ(book.bestBid(), 0.0);
    EXPECT_EQ(book.bestAsk(), 0.0);
}

// Test adding a limit order
TEST(OrderBookTest, AddLimitOrder) {
    OrderBook book;
    
    // Create a Buy Order
    Order buy_order{
        .order_id = 1,
        .side = Side::BUY,
        .type = OrderType::LIMIT,
        .price = 100.50,
        .quantity = 10,
        .timestamp = 1000
    };
    
    auto trades = book.addLimitOrder(buy_order);
    EXPECT_TRUE(trades.empty()); // No matching orders, so trades should be empty
    
    // bestBid should now be 100.50
    EXPECT_EQ(book.bestBid(), 100.50);
}

// Test cancelling an order
TEST(OrderBookTest, CancelOrder) {
    OrderBook book;
    
    Order buy_order{
        .order_id = 1,
        .side = Side::BUY,
        .type = OrderType::LIMIT,
        .price = 100.50,
        .quantity = 10,
        .timestamp = 1000
    };
    
    book.addLimitOrder(buy_order);
    EXPECT_EQ(book.bestBid(), 100.50);
    
    bool cancelled = book.cancelOrder(1);
    EXPECT_TRUE(cancelled);
    
    // Best bid should revert to 0 if the only order is cancelled
    EXPECT_EQ(book.bestBid(), 0.0);
}
