#include <gtest/gtest.h>
#include "engine/order_book.hpp"
#include "bots/avellaneda_stoikov_bot.hpp"
#include "bots/stoikov_microprice_bot.hpp"
#include <memory>

using namespace std;

TEST(BotTest, AvellanedaStoikovBotInitialization) {
    auto book = make_shared<OrderBook>();
    AvellanedaStoikovBot bot(book, 1.0, 0.05, 1.5, 1.0);
    
    EXPECT_EQ(bot.getInventory(), 0);
    EXPECT_EQ(bot.getCash(), 0.0);
    
    // With empty book, mid price is 0, bots should not quote
    bot.onTick(1.0);
    EXPECT_EQ(bot.getActiveBidQty(), 0);
    EXPECT_EQ(bot.getActiveAskQty(), 0);
}

TEST(BotTest, StoikovMicropriceBotInitialization) {
    auto book = make_shared<OrderBook>();
    StoikovMicropriceBot bot(book, 1.0, 0.05, 1.5, 1.0);
    
    EXPECT_EQ(bot.getInventory(), 0);
    EXPECT_EQ(bot.getCash(), 0.0);
    
    // With empty book, mid price is 0, bots should not quote
    bot.onTick(1.0);
    EXPECT_EQ(bot.getActiveBidQty(), 0);
    EXPECT_EQ(bot.getActiveAskQty(), 0);
}

TEST(BotTest, QuotesWhenBookHasOrders) {
    auto book = make_shared<OrderBook>();
    
    // Add dummy orders to establish a spread
    Order bid{1, Side::BUY, OrderType::LIMIT, 100.0, 10, 0};
    Order ask{2, Side::SELL, OrderType::LIMIT, 101.0, 10, 0};
    book->addLimitOrder(bid);
    book->addLimitOrder(ask);
    
    AvellanedaStoikovBot bot(book, 1.0, 0.05, 1.5, 1.0);
    bot.onTick(1.0);
    
    // The bot should have placed quotes
    EXPECT_GT(bot.getActiveBidQty(), 0);
    EXPECT_GT(bot.getActiveAskQty(), 0);
    
    StoikovMicropriceBot imb_bot(book, 1.0, 0.05, 1.5, 1.0);
    imb_bot.onTick(1.0);
    
    EXPECT_GT(imb_bot.getActiveBidQty(), 0);
    EXPECT_GT(imb_bot.getActiveAskQty(), 0);
}
