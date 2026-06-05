#include <gtest/gtest.h>
#include <tuple>
#include <vector>
#include <lowlat/book/order_book.hpp>
#include <lowlat/book/commodity_book_1.hpp>
#include <lowlat/book/commodity_book_2.hpp>
#include <lowlat/book/commodity_book_3.hpp>
#include <lowlat/book/commodity_book_4.hpp>
#include <lowlat/book/commodity_book_5.hpp>
#include <lowlat/book/commodity_book_6.hpp>

using namespace lowlat::book;
using TOB = std::tuple<Price, Shares, Price, Shares>;

// Run a fixed scenario on a book and record top_of_book after each mutation.
template <typename CB>
static std::vector<TOB> run_scenario() {
    OrderBook<CB> book;
    std::vector<TOB> snaps;
    constexpr Stock S = 0;

    auto snap = [&]() {
        snaps.push_back((*book.stock_to_book)[S].top_of_book());
    };

    book.AddOrder(S, 1, 100, 200, Side::Bid); snap();  // best bid 200
    book.AddOrder(S, 2, 50,  210, Side::Bid); snap();  // best bid 210
    book.AddOrder(S, 3, 80,  300, Side::Ask); snap();  // best ask 300
    book.AddOrder(S, 4, 60,  290, Side::Ask); snap();  // best ask 290
    book.AddOrder(S, 5, 30,  210, Side::Bid); snap();  // vol at 210 -> 80
    book.ReduceOrder(2, 30);                  snap();  // vol at 210 -> 50 (order 2: 20 shares left)
    book.ReduceOrder(2, 10);                  snap();  // vol at 210 -> 40 (order 2: 10 shares left)
    book.DeleteOrder(2);                      snap();  // order 2 gone, level 210 has 30 (order 5 only)
    book.ReplaceOrder(3, 6, 285, 80);         snap();  // ask 300 gone, new ask 285
    book.DeleteOrder(4);                      snap();  // ask 290 gone, best ask 285
    book.DeleteOrder(5);                      snap();  // bid 210 still gone
    book.DeleteOrder(1);                      snap();  // bids empty
    book.DeleteOrder(6);                      snap();  // asks empty

    return snaps;
}

TEST(CrossVariant, AllVariantsAgreeOnTopOfBook) {
    auto ref = run_scenario<CommodityBookV1>();
    ASSERT_EQ(run_scenario<CommodityBookV2>(), ref) << "V2 diverges from V1";
    ASSERT_EQ(run_scenario<CommodityBookV3>(), ref) << "V3 diverges from V1";
    ASSERT_EQ(run_scenario<CommodityBookV4>(), ref) << "V4 diverges from V1";
    ASSERT_EQ(run_scenario<CommodityBookV5>(), ref) << "V5 diverges from V1";
    ASSERT_EQ(run_scenario<CommodityBookV6>(), ref) << "V6 diverges from V1";
}

TEST(CrossVariant, SnapshotValues_Correct) {
    auto snaps = run_scenario<CommodityBookV6>();

    // After adding bid@200 only
    EXPECT_EQ(std::get<0>(snaps[0]), 200u);
    // After adding bid@210 — best bid becomes 210
    EXPECT_EQ(std::get<0>(snaps[1]), 210u);
    // After adding ask@300
    EXPECT_EQ(std::get<2>(snaps[2]), 300u);
    // After adding ask@290 — best ask becomes 290
    EXPECT_EQ(std::get<2>(snaps[3]), 290u);
    // After deleting order 2 — order 5 still at 210, best bid remains 210
    EXPECT_EQ(std::get<0>(snaps[7]), 210u);
    // After deleting order 5 — level 210 gone, best bid drops to 200
    EXPECT_EQ(std::get<0>(snaps[10]), 200u);
    // After replace ask@300 with ask@285 — best ask becomes 285
    EXPECT_EQ(std::get<2>(snaps[8]), 285u);
    // After all orders gone — all zeros
    EXPECT_EQ(snaps.back(), TOB(0, 0, 0, 0));
}
