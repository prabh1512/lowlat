#pragma once

#include <cstdint>
#include <lowlat/itch/messages.hpp>

namespace lowlat::itch {

struct CountingHandler {
    std::uint64_t adds         = 0;
    std::uint64_t add_attribs  = 0;
    std::uint64_t executes     = 0;
    std::uint64_t executes_p   = 0;
    std::uint64_t cancels      = 0;
    std::uint64_t deletes      = 0;
    std::uint64_t replaces     = 0;

    void operator()(const AddOrder&)               { ++adds; }
    void operator()(const AddOrderAttributed&)     { ++add_attribs; }
    void operator()(const OrderExecuted&)          { ++executes; }
    void operator()(const OrderExecutedWithPrice&) { ++executes_p; }
    void operator()(const OrderCancel&)            { ++cancels; }
    void operator()(const OrderDelete&)            { ++deletes; }
    void operator()(const OrderReplace&)           { ++replaces; }
};

}  // namespace lowlat::itch