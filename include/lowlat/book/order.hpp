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

struct Order {
    Stock     stock;
    OrderId   order_id;
    Shares    shares;
    Price     price;
    Side      side;
    std::uint32_t prev;
    std::uint32_t next;
    std::uint32_t level_idx; 
};

} // namespace lowlat::book