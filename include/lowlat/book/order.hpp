#pragma once

#include <cstdint>
#include <limits>

namespace lowlat::book {

using Stock = std::uint16_t;
using OrderId = std::uint64_t;
using Shares = std::uint32_t;
using Price = std::uint32_t;
enum class Side : std::uint8_t { Bid, Ask };

inline constexpr std::uint32_t NIL = UINT32_MAX; // to represent NIL in the linked lists.

struct Order{
    Stock stock = std::numeric_limits<Stock>::max();
    OrderId order_id = std::numeric_limits<OrderId>::max();
    Shares shares = std::numeric_limits<Shares>::max();
    Price price = std::numeric_limits<Price>::max();
    Side side = Side::Bid;
    uint32_t next = NIL;
    uint32_t prev = NIL;
};

} // namespace lowlat::book