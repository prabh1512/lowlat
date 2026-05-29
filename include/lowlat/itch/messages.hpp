// ITCH 5.0 message definitions.
//
// Each message struct mirrors the wire format byte-for-byte. Multi-byte
// integers are stored in big-endian order (as they appear on the wire) and
// must be converted to host byte order before use.
//
// Reference: NASDAQ TotalView-ITCH 5.0 specification.

#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <type_traits>

namespace lowlat::itch {

using Price = std::uint32_t;
using OrderId = std::uint64_t;
using Timestamp = std::uint64_t;
using Shares = std::uint32_t;
using StockSymbol = std::array<char, 8>;
inline constexpr std::uint32_t PRICE_SCALE = 10000;


#pragma pack(push, 1)
template<typename T>
struct BigEndian{
    static_assert(std::is_unsigned_v<T>, "ITCH integer fields are unsigned");
    T val;
    [[ nodiscard ]] T get() const noexcept {
        if constexpr (std::endian::native == std::endian::little){
            return std::byteswap(val);
        }
        return val;
    }
};
#pragma pack(pop)

using be16 = BigEndian<std::uint16_t>;
using be32 = BigEndian<std::uint32_t>;
using be64 = BigEndian<std::uint64_t>;

#pragma pack(push, 1)
struct Timestamp48 {
    std::uint8_t bytes[6];

    [[nodiscard]] std::uint64_t get() const noexcept {
        // assemble the 6 big-endian bytes into a uint64_t
        uint64_t res = 0;
        for (int i = 0; i < 6; i ++){
            res += bytes[i]*(1LL << (8*(5-i)));
        }
        return res;
    }
};
#pragma pack(pop)

static_assert(sizeof(Timestamp48) == 6, "Timestamp48 must be exactly 6 bytes");

#pragma pack(push, 1)

// 'A' — Add Order, No MPID Attribution. Wire size: 36 bytes.
struct AddOrder {
    char  message_type;       // 'A'
    be16  stock_locate;
    be16  tracking_number;
    Timestamp48  timestamp;
    be64  order_ref;
    char buy_sell;           // 'B' or 'S'
    be32 shares;
    char stock[8];              // 8 ASCII chars
    be32 price;
};
static_assert(sizeof(AddOrder) == 36, "AddOrder wire size");

// 'F' — Add Order with MPID Attribution. Wire size: 40 bytes.
// Same as AddOrder plus a 4-byte attribution field at the end.
struct AddOrderAttributed {
    char        message_type;       // 'F'
    be16  stock_locate;
    be16  tracking_number;
    Timestamp48  timestamp;
    be64  order_ref;
    char        buy_sell;
    be32  shares;
    char  stock[8];              // 8 ASCII chars
    be32  price;
    char  attribution[4];        // 4 ASCII chars (MPID)
};
static_assert(sizeof(AddOrderAttributed) == 40, "AddOrderAttributed wire size");

// 'E' — Order Executed. Wire size: 31 bytes.
//   message_type:1, stock_locate:2, tracking_number:2, timestamp:6,
//   order_ref:8, executed_shares:4, match_number:8
struct OrderExecuted {
    char        message_type;       // 'E'
    be16  stock_locate;
    be16  tracking_number;
    Timestamp48  timestamp;
    be64  order_ref;
    be32  executed_shares;
    be64  match_number;       // 8-byte integer
};
static_assert(sizeof(OrderExecuted) == 31, "OrderExecuted wire size");

// 'C' — Order Executed With Price. Wire size: 36 bytes.
//   ...OrderExecuted fields... + printable:1(char) + execution_price:4
struct OrderExecutedWithPrice {
    char        message_type;       // 'C'
    be16  stock_locate;
    be16  tracking_number;
    Timestamp48  timestamp;
    be64  order_ref;
    be32  executed_shares;
    be64  match_number; 
    char        printable;          // 'Y' or 'N'
    be32  execution_price;
};
static_assert(sizeof(OrderExecutedWithPrice) == 36, "OrderExecutedWithPrice wire size");

// 'X' — Order Cancel. Wire size: 23 bytes.
//   message_type:1, stock_locate:2, tracking_number:2, timestamp:6,
//   order_ref:8, cancelled_shares:4
struct OrderCancel {
    char        message_type;       // 'X'
    be16         stock_locate;
    be16         tracking_number;
    Timestamp48  timestamp;
    be64         order_ref;
    be32         cancelled_shares;

};
static_assert(sizeof(OrderCancel) == 23, "OrderCancel wire size");

// 'D' — Order Delete. Wire size: 19 bytes.
//   message_type:1, stock_locate:2, tracking_number:2, timestamp:6, order_ref:8
struct OrderDelete {
    char        message_type;       // 'D'
    be16         stock_locate;
    be16         tracking_number;
    Timestamp48  timestamp;
    be64         order_ref;
};
static_assert(sizeof(OrderDelete) == 19, "OrderDelete wire size");

// 'U' — Order Replace. Wire size: 35 bytes.
//   message_type:1, stock_locate:2, tracking_number:2, timestamp:6,
//   original_order_ref:8, new_order_ref:8, shares:4, price:4
struct OrderReplace {
    char        message_type;       // 'U'
    be16         stock_locate;
    be16         tracking_number;
    Timestamp48  timestamp;
    be64         original_order_ref;
    be64         new_order_ref;
    be32         shares;
    be32         price;
};
static_assert(sizeof(OrderReplace) == 35, "OrderReplace wire size");

#pragma pack(pop)


}  // namespace lowlat::itch
