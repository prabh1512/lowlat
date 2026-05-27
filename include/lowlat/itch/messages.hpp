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

namespace lowlat::itch {

// -----------------------------------------------------------------------------
// Domain types
//
// We use distinct type aliases for fields with different semantic meaning, even
// when they share an underlying representation. This makes APIs self-documenting
// (you can't accidentally pass an OrderId where a Price is expected, modulo C++
// alias rules — these are not strong typedefs, just signals to the reader).
// -----------------------------------------------------------------------------

// Price in fixed-point format with 4 implied decimal places.
// On the wire: 4-byte big-endian unsigned integer.
// Semantic: price_in_dollars = Price / 10000.
using Price = std::uint32_t;

// Order reference number, unique per trading day.
// On the wire: 8-byte big-endian unsigned integer.
using OrderId = std::uint64_t;

// Nanoseconds since midnight.
// On the wire: 6-byte big-endian unsigned integer (stored in a 48-bit field).
// We promote to 64-bit in code for convenience.
using Timestamp = std::uint64_t;

// Number of shares.
// On the wire: 4-byte big-endian unsigned integer.
using Shares = std::uint32_t;

// Stock symbol: 8 ASCII characters, left-justified, space-padded.
using StockSymbol = std::array<char, 8>;

// Scaling factor for converting Price to dollars.
inline constexpr std::uint32_t PRICE_SCALE = 10000;

}  // namespace lowlat::itch
