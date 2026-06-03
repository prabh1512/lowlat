#pragma once

#include <cstdint>
#include <lowlat/book/order.hpp>
#include <lowlat/book/order_book.hpp>
#include <lowlat/core/tsc.hpp>
#include <lowlat/itch/messages.hpp>
#include <vector>

namespace lowlat::book {

template <typename CB> struct BookHandler {
  OrderBook<CB> &book;

  std::vector<std::uint32_t> add_cycles;
  std::vector<std::uint32_t> reduce_cycles;
  std::vector<std::uint32_t> delete_cycles;
  std::vector<std::uint32_t> replace_cycles;

  explicit BookHandler(OrderBook<CB> &b) : book(b) {
    add_cycles.reserve(60'000'000);
    reduce_cycles.reserve(5'000'000);
    delete_cycles.reserve(55'000'000);
    replace_cycles.reserve(10'000'000);
  }

  void operator()(const itch::AddOrder &msg) {
    std::uint64_t t0 = core::rdtsc();
    book.AddOrder(msg.stock_locate.get(), msg.order_ref.get(), msg.shares.get(),
                  msg.price.get(),
                  (msg.buy_sell == 'B') ? Side::Bid : Side::Ask);
    std::uint64_t t1 = core::rdtsc();
    add_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }
  void operator()(const itch::AddOrderAttributed &msg) {
    std::uint64_t t0 = core::rdtsc();
    book.AddOrder(msg.stock_locate.get(), msg.order_ref.get(), msg.shares.get(),
                  msg.price.get(),
                  (msg.buy_sell == 'B') ? Side::Bid : Side::Ask);
    std::uint64_t t1 = core::rdtsc();
    add_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }
  void operator()(const itch::OrderExecuted &msg) {
    std::uint64_t t0 = core::rdtsc();
    book.ReduceOrder(msg.order_ref.get(), msg.executed_shares.get());
    std::uint64_t t1 = core::rdtsc();
    reduce_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }

  void operator()(const itch::OrderExecutedWithPrice &msg) {
    std::uint64_t t0 = core::rdtsc();
    book.ReduceOrder(msg.order_ref.get(), msg.executed_shares.get());
    std::uint64_t t1 = core::rdtsc();
    reduce_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }

  void operator()(const itch::OrderCancel &msg) {
    std::uint64_t t0 = core::rdtsc();
    book.ReduceOrder(msg.order_ref.get(), msg.cancelled_shares.get());
    std::uint64_t t1 = core::rdtsc();
    reduce_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }

  void operator()(const itch::OrderDelete &msg) {
    std::uint64_t t0 = core::rdtsc();
    book.DeleteOrder(msg.order_ref.get());
    std::uint64_t t1 = core::rdtsc();
    delete_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }

  void operator()(const itch::OrderReplace &msg) {
    std::uint64_t t0 = core::rdtsc();
    book.ReplaceOrder(msg.original_order_ref.get(), msg.new_order_ref.get(),
                      msg.price.get(), msg.shares.get());
    std::uint64_t t1 = core::rdtsc();
    replace_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }
};

} // namespace lowlat::book