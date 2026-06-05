#include <gtest/gtest.h>
#include <lowlat/book/order_book.hpp>
#include <lowlat/book/commodity_book_1.hpp>
#include <lowlat/book/commodity_book_2.hpp>
#include <lowlat/book/commodity_book_3.hpp>
#include <lowlat/book/commodity_book_4.hpp>
#include <lowlat/book/commodity_book_5.hpp>
#include <lowlat/book/commodity_book_6.hpp>

using namespace lowlat::book;

template <typename CB>
class OrderBookTest : public ::testing::Test {
protected:
    OrderBook<CB> book;
    static constexpr Stock  S   = 1;
    static constexpr Price  P1  = 100, P2 = 200, P3 = 150;
    static constexpr Shares SH  = 50;

    void TearDown() override {
        CB::add_cycles.clear();
        CB::reduce_cycles.clear();
    }
};

using CBTypes = ::testing::Types<
    CommodityBookV1, CommodityBookV2, CommodityBookV3,
    CommodityBookV4, CommodityBookV5, CommodityBookV6>;
TYPED_TEST_SUITE(OrderBookTest, CBTypes);

// helpers
static auto tob_bid(auto& book, Stock s) {
    auto [bp, bs, ap, as] = (*book.stock_to_book)[s].top_of_book();
    return std::make_pair(bp, bs);
}
static auto tob_ask(auto& book, Stock s) {
    auto [bp, bs, ap, as] = (*book.stock_to_book)[s].top_of_book();
    return std::make_pair(ap, as);
}

TYPED_TEST(OrderBookTest, EmptyBook) {
    auto [bp, bs, ap, as] = (*this->book.stock_to_book)[this->S].top_of_book();
    EXPECT_EQ(bp, 0u); EXPECT_EQ(bs, 0u);
    EXPECT_EQ(ap, 0u); EXPECT_EQ(as, 0u);
}

TYPED_TEST(OrderBookTest, SingleBid) {
    this->book.AddOrder(this->S, 1, this->SH, this->P1, Side::Bid);
    auto [bp, bs] = tob_bid(this->book, this->S);
    EXPECT_EQ(bp, this->P1);
    EXPECT_EQ(bs, this->SH);
}

TYPED_TEST(OrderBookTest, SingleAsk) {
    this->book.AddOrder(this->S, 1, this->SH, this->P1, Side::Ask);
    auto [ap, as] = tob_ask(this->book, this->S);
    EXPECT_EQ(ap, this->P1);
    EXPECT_EQ(as, this->SH);
}

TYPED_TEST(OrderBookTest, BestBidIsHighest) {
    // add bids at P1=100, P2=200, P3=150 — best must be P2
    this->book.AddOrder(this->S, 1, this->SH, this->P1, Side::Bid);
    this->book.AddOrder(this->S, 2, this->SH, this->P2, Side::Bid);
    this->book.AddOrder(this->S, 3, this->SH, this->P3, Side::Bid);
    auto [bp, bs] = tob_bid(this->book, this->S);
    EXPECT_EQ(bp, this->P2);
    EXPECT_EQ(bs, this->SH);
}

TYPED_TEST(OrderBookTest, BestAskIsLowest) {
    // add asks at P2=200, P3=150, P1=100 — best must be P1
    this->book.AddOrder(this->S, 1, this->SH, this->P2, Side::Ask);
    this->book.AddOrder(this->S, 2, this->SH, this->P3, Side::Ask);
    this->book.AddOrder(this->S, 3, this->SH, this->P1, Side::Ask);
    auto [ap, as] = tob_ask(this->book, this->S);
    EXPECT_EQ(ap, this->P1);
    EXPECT_EQ(as, this->SH);
}

TYPED_TEST(OrderBookTest, VolumeAccumulatesAtSameLevel) {
    this->book.AddOrder(this->S, 1, 30u, this->P1, Side::Bid);
    this->book.AddOrder(this->S, 2, 20u, this->P1, Side::Bid);
    auto [bp, bs] = tob_bid(this->book, this->S);
    EXPECT_EQ(bp, this->P1);
    EXPECT_EQ(bs, 50u);
}

TYPED_TEST(OrderBookTest, PartialReduceUpdatesVolume) {
    this->book.AddOrder(this->S, 1, this->SH, this->P1, Side::Bid);
    this->book.ReduceOrder(1, 20u);
    auto [bp, bs] = tob_bid(this->book, this->S);
    EXPECT_EQ(bp, this->P1);
    EXPECT_EQ(bs, 30u);
}

TYPED_TEST(OrderBookTest, FullReduceRemovesLevel) {
    // two levels; fully reduce the better one — top drops to next
    this->book.AddOrder(this->S, 1, this->SH, this->P2, Side::Bid); // best
    this->book.AddOrder(this->S, 2, this->SH, this->P1, Side::Bid); // second
    this->book.ReduceOrder(1, this->SH); // exhaust level P2
    auto [bp, bs] = tob_bid(this->book, this->S);
    EXPECT_EQ(bp, this->P1);
    EXPECT_EQ(bs, this->SH);
}

TYPED_TEST(OrderBookTest, DeleteOrderUpdatesTop) {
    this->book.AddOrder(this->S, 1, this->SH, this->P2, Side::Ask); // best ask
    this->book.AddOrder(this->S, 2, this->SH, this->P3, Side::Ask);
    this->book.DeleteOrder(1);
    auto [ap, as] = tob_ask(this->book, this->S);
    EXPECT_EQ(ap, this->P3);
    EXPECT_EQ(as, this->SH);
}

TYPED_TEST(OrderBookTest, ReplaceOrderNewPriceReflected) {
    this->book.AddOrder(this->S, 1, this->SH, this->P1, Side::Bid);
    this->book.ReplaceOrder(1, 2, this->P2, this->SH);
    auto [bp, bs] = tob_bid(this->book, this->S);
    EXPECT_EQ(bp, this->P2); // new (higher) price is now best
    EXPECT_EQ(bs, this->SH);
}

TYPED_TEST(OrderBookTest, BookEmptyAfterAllDeleted) {
    this->book.AddOrder(this->S, 1, this->SH, this->P1, Side::Bid);
    this->book.AddOrder(this->S, 2, this->SH, this->P1, Side::Ask);
    this->book.DeleteOrder(1);
    this->book.DeleteOrder(2);
    auto [bp, bs, ap, as] = (*this->book.stock_to_book)[this->S].top_of_book();
    EXPECT_EQ(bp, 0u); EXPECT_EQ(bs, 0u);
    EXPECT_EQ(ap, 0u); EXPECT_EQ(as, 0u);
}

TYPED_TEST(OrderBookTest, IndependentStocks) {
    this->book.AddOrder(0, 1, this->SH, this->P1, Side::Bid);
    this->book.AddOrder(1, 2, this->SH, this->P2, Side::Bid);
    EXPECT_EQ(tob_bid(this->book, 0).first, this->P1);
    EXPECT_EQ(tob_bid(this->book, 1).first, this->P2);
}
