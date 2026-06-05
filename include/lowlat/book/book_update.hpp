#pragma once

#include <cstdint>
#include <lowlat/book/order.hpp>

namespace lowlat::book {

struct BookUpdate {
    std::uint64_t ts_tsc;
    Stock         stock;
    Price         best_bid_price;
    Shares        best_bid_size;
    Price         best_ask_price;
    Shares        best_ask_size;
};

}  // namespace lowlat::book