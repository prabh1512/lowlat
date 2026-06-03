#pragma once

#include <algorithm>
#include <lowlat/book/order.hpp>
#include <lowlat/core/tsc.hpp>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lowlat::book {

inline constexpr uint32_t ZERO = 0;

struct CommodityBookV2 {

  std::vector<std::pair<Price, Shares>> BidLevels;
  std::vector<std::pair<Price, Shares>> AskLevels;

  // BidLevels stores largest to smallest bids
  // Asklevels stores smallest to largest bids

  std::unordered_map<Price, std::pair<uint32_t, uint32_t>> BidHT;
  std::unordered_map<Price, std::pair<uint32_t, uint32_t>> AskHT;
  static inline std::vector<std::uint32_t> add_cycles;
  static inline std::vector<std::uint32_t> reduce_cycles;

  template <typename Levels, typename HT, typename Compare>
  void Add(Levels &levels, HT &ht, Compare comp, Price price, Shares shares,
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
    auto c = [&](const auto &a, const auto &b) {
      return comp(a.first, b.first);
    };

    auto it = std::lower_bound(levels.begin(), levels.end(),
                               std::make_pair(price, ZERO), c);
    if (it == levels.end() || (it->first != price)) [[unlikely]] {
      levels.insert(it, std::make_pair(price, shares));
    } else [[likely]] {
      it->second += shares;
    }
  }

  template <typename Levels, typename HT, typename Compare>
  bool Reduce(Levels &levels, HT &ht, Compare comp, Price price, Shares delta,
              std::uint32_t idx, OrderPool &pool) {
    Order &order = pool.pool[idx];

    if (delta > order.shares) [[unlikely]] {
      throw std::runtime_error("reduce: delta exceeds order shares");
    }

    order.shares -= delta;

    auto c = [&](const auto &a, const auto &b) {
      return comp(a.first, b.first);
    };

    auto it = std::lower_bound(levels.begin(), levels.end(),
                               std::make_pair(price, ZERO), c);
    if (it == levels.end() || (it->first != price)) [[unlikely]] {
      throw std::runtime_error("appropriate price level does not exist");
    }
    if (it->second < delta) [[unlikely]] {
      throw std::runtime_error("delta exceeds volume in price level");
    }

    it->second -= delta;

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

      if (it->second == 0) {
        levels.erase(it);
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
      Add(BidLevels, BidHT, std::greater<Price>(), price, shares, idx, pool);
    } else {
      Add(AskLevels, AskHT, std::less<Price>(), price, shares, idx, pool);
    }
    std::uint64_t t1 = core::rdtsc();
    add_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }

  template <Side S>
  bool Reduce(Price price, Shares delta, std::uint32_t idx, OrderPool &pool) {
    std::uint64_t t0 = core::rdtsc();
    bool removed;
    if constexpr (S == Side::Bid) {
      removed = Reduce(BidLevels, BidHT, std::greater<Price>(), price, delta,
                       idx, pool);
    } else {
      removed =
          Reduce(AskLevels, AskHT, std::less<Price>(), price, delta, idx, pool);
    }
    std::uint64_t t1 = core::rdtsc();
    reduce_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
    return removed;
  }
};

} // namespace lowlat::book
