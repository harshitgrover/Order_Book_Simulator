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

// Test ModifyOrder - Queue Priority Preserved
TEST(OrderBookTest, ModifyOrderDecreaseQty) {
    OrderBook book;
    
    Order buy1{.order_id=1, .side=Side::BUY, .type=OrderType::LIMIT, .price=100.0, .quantity=10, .timestamp=1};
    Order buy2{.order_id=2, .side=Side::BUY, .type=OrderType::LIMIT, .price=100.0, .quantity=10, .timestamp=2};
    book.addLimitOrder(buy1);
    book.addLimitOrder(buy2);
    
    // Modify first order by decreasing quantity (Queue Priority should be PRESERVED)
    book.modifyOrder(1, 5);
    
    Order sell_market{.order_id=3, .side=Side::SELL, .type=OrderType::MARKET, .price=0.0, .quantity=10, .timestamp=3};
    auto trades = book.addMarketOrder(sell_market);
    
    // Should fill 5 from buy1, then 5 from buy2
    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].buy_order_id, 1);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(trades[1].buy_order_id, 2);
    EXPECT_EQ(trades[1].quantity, 5);
}

// Test ModifyOrder - Queue Priority Lost
TEST(OrderBookTest, ModifyOrderIncreaseQty) {
    OrderBook book;
    
    Order buy1{.order_id=1, .side=Side::BUY, .type=OrderType::LIMIT, .price=100.0, .quantity=10, .timestamp=1};
    Order buy2{.order_id=2, .side=Side::BUY, .type=OrderType::LIMIT, .price=100.0, .quantity=10, .timestamp=2};
    book.addLimitOrder(buy1);
    book.addLimitOrder(buy2);
    
    // Modify first order by increasing quantity (Queue Priority should be LOST, moves to back of queue)
    book.modifyOrder(1, 15);
    
    Order sell_market{.order_id=3, .side=Side::SELL, .type=OrderType::MARKET, .price=0.0, .quantity=10, .timestamp=3};
    auto trades = book.addMarketOrder(sell_market);
    
    // Since buy1 lost priority, buy2 should be at the front of the queue
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].buy_order_id, 2);
    EXPECT_EQ(trades[0].quantity, 10);
}
