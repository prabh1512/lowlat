// ITCH 5.0 message dispatcher.
//
// Given a buffer pointing at the start of one ITCH message, reads the type
// byte, reinterprets the buffer as the corresponding message struct, and
// invokes the handler with that struct.
//
// The handler is a template parameter so the call is inlined — no virtual
// dispatch, no std::function, no std::variant copy. The switch on message
// type compiles to a jump table.
//
// Returns the wire size of the dispatched message, so a streaming loop can
// advance past it. Returns 0 for unknown / unsupported message types.

#pragma once

#include <cstddef>
#include <utility>            // std::forward
#include <lowlat/itch/messages.hpp>
#include <lowlat/core/compiler.hpp>

namespace lowlat::itch {

template <typename Handler>
LOWLAT_ALWAYS_INLINE
std::size_t dispatch(const std::byte* buf, Handler&& handler) {
    // TODO: read buf[0] as a char, switch on it, for each known type:
    //   - reinterpret_cast buf to const <MsgType>*
    //   - call handler(*ptr) — using std::forward<Handler>(handler) if needed
    //   - return sizeof(<MsgType>)
    // default case: return 0 (unknown type, caller decides what to do)
    const char c = static_cast<const char>(buf[0]);
    switch (c){
        case 'A': {
            const auto* ptr = reinterpret_cast<const AddOrder*>(buf);
            handler(*ptr);
            return 36;
        }template <typename Handler>
LOWLAT_ALWAYS_INLINE
std::size_t dispatch(const std::byte* buf, Handler&& handler) {
    const char c = static_cast<char>(buf[0]);
    switch (c) {
        case 'A': {
            const auto* ptr = reinterpret_cast<const AddOrder*>(buf);
            handler(*ptr);
            return sizeof(AddOrder);
        }
        case 'F': {
            const auto* ptr = reinterpret_cast<const AddOrderAttributed*>(buf);
            handler(*ptr);
            return sizeof(AddOrderAttributed);
        }
        case 'E': {
            const auto* ptr = reinterpret_cast<const OrderExecuted*>(buf);
            handler(*ptr);
            return sizeof(OrderExecuted);
        }
        case 'C': {
            const auto* ptr = reinterpret_cast<const OrderExecutedWithPrice*>(buf);
            handler(*ptr);
            return sizeof(OrderExecutedWithPrice);
        }
        case 'X': {
            const auto* ptr = reinterpret_cast<const OrderCancel*>(buf);
            handler(*ptr);
            return sizeof(OrderCancel);
        }
        case 'D': {
            const auto* ptr = reinterpret_cast<const OrderDelete*>(buf);
            handler(*ptr);
            return sizeof(OrderDelete);
        }
        case 'U': {
            const auto* ptr = reinterpret_cast<const OrderReplace*>(buf);
            handler(*ptr);
            return sizeof(OrderReplace);
        }
        default:
            return 0;
    }
}
    }
}

}  // namespace lowlat::itch