#pragma once

#include <algorithm>
#include <cstdint>
#include <lowlat/book/commodity_book_1.hpp>
#include <lowlat/book/commodity_book_2.hpp>
#include <lowlat/book/commodity_book_3.hpp>
#include <lowlat/book/commodity_book_4.hpp>
#include <lowlat/book/commodity_book_6.hpp>
#include <lowlat/book/order.hpp>
#include <lowlat/core/tsc.hpp>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include <tuple>
#include <limits>

namespace lowlat::book {

struct CommodityBookV5 {

  std::vector<std::pair<Price, Shares>> BidLevels;
  std::vector<std::pair<Price, Shares>> AskLevels;

  // BidLevels stores smallest to largest bids -> more activity happens at the
  // back, and operations are cheaper Asklevels stores largest to smallest bids
  // -> same reason as above

  std::unordered_map<Price, std::pair<uint32_t, uint32_t>> BidHT;
  std::unordered_map<Price, std::pair<uint32_t, uint32_t>> AskHT;
  static inline std::vector<std::uint32_t> add_cycles;
  static inline std::vector<std::uint32_t> reduce_cycles;

  template <typename Levels, typename HT>
  void Add(Levels &levels, HT &ht, Price price, Shares shares,
           std::uint32_t idx, OrderPool &pool) {
    if (ht.find(price) == ht.end())
      ht[price] = std::make_pair(NIL, NIL);
    auto &[u, v] = ht[price];
    if (v != NIL)
      pool.pool[v].next = idx;
    pool.pool[idx].prev = v;
    pool.pool[idx].next = NIL;
    v = idx;
    if (u == NIL)
      u = idx;
    // auto c = [&](const auto& a, const auto& b) {
    //     return comp(a.first, b.first);
    // };

    // auto it = branchless_lower_bound(levels.begin(), levels.end(),
    // std::make_pair(price, ZERO), c); if (it == levels.end() || (it->first !=
    // price)){
    //     levels.insert(it, std::make_pair(price, shares));
    // }
    // else{
    //     it->second += shares;
    // }

    bool done = false;
    for (auto &x : levels) {
      if (x.first == price) {
        x.second += shares;
        done = true;
        break;
      }
    }
    if (done)
      return;
    levels.emplace_back(price, shares);
  }

  template <typename Levels, typename HT>
  bool Reduce(Levels &levels, HT &ht, Price price, Shares delta,
              std::uint32_t idx, OrderPool &pool) {
    Order &order = pool.pool[idx];

    if (delta > order.shares) {
      throw std::runtime_error("reduce: delta exceeds order shares");
    }

    order.shares -= delta;

    // auto c = [&](const auto& a, const auto& b) {
    //     return comp(a.first, b.first);
    // };

    std::uint64_t tot_shares = 0;
    std::uint32_t indx = 0;

    for (auto &x : levels) {
      if (x.first == price) {
        tot_shares = x.second;
        break;
      }
      ++indx;
    }

    if (tot_shares == 0) {
      throw std::runtime_error("price level not found");
    }

    if (tot_shares < delta) {
      throw std::runtime_error("delta exceeds volume in price level");
    }

    levels[indx].second -= delta;

    // auto it = branchless_lower_bound(levels.begin(), levels.end(),
    // std::make_pair(price, ZERO), c); if (it == levels.end() || (it->first !=
    // price)){
    //     throw std::runtime_error("appropriate price level does not exist");
    // }
    // if (it->second < delta){
    //     throw std::runtime_error("delta exceeds volume in price level");
    // }

    // it->second -= delta;

    if (order.shares == 0) {
      auto &[head, tail] = ht[price];
      if (order.prev != NIL)
        pool.pool[order.prev].next = order.next;
      else
        head = order.next;
      if (order.next != NIL)
        pool.pool[order.next].prev = order.prev;
      else
        tail = order.prev;

      if (tot_shares == delta) {
        levels.erase(levels.begin() + indx);
        ht.erase(price);
      }

      return true;
    }
    return false;
  }

  template <Side S>
  void Add(Price price, Shares shares, std::uint32_t idx, OrderPool &pool) {
    std::uint64_t t0 = core::rdtsc();
    if constexpr (S == Side::Bid) {
      Add(BidLevels, BidHT, price, shares, idx, pool);
    } else {
      Add(AskLevels, AskHT, price, shares, idx, pool);
    }
    std::uint64_t t1 = core::rdtsc();
    add_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }

  template <Side S>
  bool Reduce(Price price, Shares delta, std::uint32_t idx, OrderPool &pool) {
    std::uint64_t t0 = core::rdtsc();
    bool removed;
    if constexpr (S == Side::Bid) {
      removed = Reduce(BidLevels, BidHT, price, delta, idx, pool);
    } else {
      removed = Reduce(AskLevels, AskHT, price, delta, idx, pool);
    }
    std::uint64_t t1 = core::rdtsc();
    reduce_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
    return removed;
  }

  std::tuple<Price, Shares, Price, Shares> top_of_book() const {
    Price bp = 0; Shares bs = 0;
    Price ap = 0; Shares as = 0;
    for (const auto& [px, sh] : BidLevels) {
        if (px > bp) { bp = px; bs = static_cast<Shares>(sh); }
    }
    Price min_ask = std::numeric_limits<Price>::max();
    for (const auto& [px, sh] : AskLevels) {
        if (px < min_ask) { min_ask = px; as = static_cast<Shares>(sh); }
    }
    if (min_ask != std::numeric_limits<Price>::max()) ap = min_ask;
    return {bp, bs, ap, as};
}

};

} // namespace lowlat::book
