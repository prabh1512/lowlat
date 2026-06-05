#pragma once

#include <algorithm>
#include <cstdint>
#include <lowlat/book/order.hpp>
#include <lowlat/book/order_pool.hpp>
#include <lowlat/core/tsc.hpp>
#include <stdexcept>
#include <vector>
#include <tuple>

namespace lowlat::book {

// PriceLevel lives in a separate pool. Its pool index is stable — never
// changes. Orders store their level's pool index for O(1) reduce/delete.
struct PriceLevel {
  Price price;
  std::uint64_t volume;
  std::uint32_t head;      // FIFO head (pool slot index)
  std::uint32_t tail;      // FIFO tail
  std::uint32_t next_free; // free list link (when not in use)
};

inline constexpr std::uint32_t MAX_LEVELS = 256;

struct CommodityBookV6 {
  // Stable pool
  std::vector<PriceLevel> level_pool;
  std::uint32_t free_level_head;

  std::vector<std::uint32_t> BidIndices;
  std::vector<std::uint32_t> AskIndices;

  static inline std::vector<std::uint32_t> add_cycles;
  static inline std::vector<std::uint32_t> reduce_cycles;

  CommodityBookV6() : level_pool(MAX_LEVELS), free_level_head(0) {
    for (std::uint32_t i = 0; i < MAX_LEVELS - 1; ++i) {
      level_pool[i].next_free = i + 1;
    }
    level_pool[MAX_LEVELS - 1].next_free = NIL;
    BidIndices.reserve(128);
    AskIndices.reserve(128);
  }

  std::uint32_t allocate_level() {
    if (free_level_head == NIL) [[unlikely]] {
      throw std::runtime_error("level pool full");
    }
    std::uint32_t idx = free_level_head;
    free_level_head = level_pool[idx].next_free;
    return idx;
  }

  void free_level(std::uint32_t idx) {
    level_pool[idx].next_free = free_level_head;
    free_level_head = idx;
  }

  template <typename Compare> auto make_idx_cmp(Compare price_cmp) {
    return [this, price_cmp](std::uint32_t idx, Price target) {
      return price_cmp(level_pool[idx].price, target);
    };
  }

  template <typename Indices, typename Compare>
  void Add(Indices &indices, Compare price_cmp, Price price, Shares shares,
           std::uint32_t order_idx, OrderPool &pool) {
    auto cmp = make_idx_cmp(price_cmp);
    auto it = std::lower_bound(indices.begin(), indices.end(), price, cmp);

    std::uint32_t lvl_pool_idx;
    if (it == indices.end() || level_pool[*it].price != price) [[unlikely]] {
      // New level: allocate, init, insert pool_idx into indices.
      lvl_pool_idx = allocate_level();
      PriceLevel &lvl = level_pool[lvl_pool_idx];
      lvl.price = price;
      lvl.volume = shares;
      lvl.head = order_idx;
      lvl.tail = order_idx;
      indices.insert(it, lvl_pool_idx);
      pool.pool[order_idx].prev = NIL;
      pool.pool[order_idx].next = NIL;
    } else [[likely]] {
      // Existing level: update volume, append to FIFO.
      lvl_pool_idx = *it;
      PriceLevel &lvl = level_pool[lvl_pool_idx];
      lvl.volume += shares;
      pool.pool[lvl.tail].next = order_idx;
      pool.pool[order_idx].prev = lvl.tail;
      pool.pool[order_idx].next = NIL;
      lvl.tail = order_idx;
    }
    pool.pool[order_idx].level_idx = lvl_pool_idx;
  }

  template <typename Indices, typename Compare>
  bool Reduce(Indices &indices, Compare price_cmp, Price /*price*/,
              Shares delta, std::uint32_t order_idx, OrderPool &pool) {
    Order &order = pool.pool[order_idx];
    if (delta > order.shares) [[unlikely]] {
      throw std::runtime_error("reduce: delta exceeds order shares");
    }
    order.shares -= delta;

    // Direct: no binary search.
    std::uint32_t lvl_pool_idx = order.level_idx;
    PriceLevel &lvl = level_pool[lvl_pool_idx];
    if (lvl.volume < delta) [[unlikely]] {
      throw std::runtime_error("delta exceeds level volume");
    }
    lvl.volume -= delta;

    if (order.shares == 0) {
      // FIFO unlink.
      if (order.prev != NIL)
        pool.pool[order.prev].next = order.next;
      else
        lvl.head = order.next;
      if (order.next != NIL)
        pool.pool[order.next].prev = order.prev;
      else
        lvl.tail = order.prev;

      if (lvl.volume == 0) {
        // Level empty: erase its pool_idx from sorted indices, free pool slot.
        auto cmp = make_idx_cmp(price_cmp);
        auto it =
            std::lower_bound(indices.begin(), indices.end(), lvl.price, cmp);
        // it should point at lvl_pool_idx (or an equivalent price). Verify.
        while (it != indices.end() && *it != lvl_pool_idx)
          ++it;
        if (it != indices.end())
          indices.erase(it);
        free_level(lvl_pool_idx);
      }
      return true;
    }
    return false;
  }

  template <Side S>
  void Add(Price price, Shares shares, std::uint32_t idx, OrderPool &pool) {
    std::uint64_t t0 = core::rdtsc();
    if constexpr (S == Side::Bid) {
      Add(BidIndices, std::greater<Price>(), price, shares, idx, pool);
    } else {
      Add(AskIndices, std::less<Price>(), price, shares, idx, pool);
    }
    std::uint64_t t1 = core::rdtsc();
    add_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
  }

  template <Side S>
  bool Reduce(Price price, Shares delta, std::uint32_t idx, OrderPool &pool) {
    std::uint64_t t0 = core::rdtsc();
    bool removed;
    if constexpr (S == Side::Bid) {
      removed =
          Reduce(BidIndices, std::greater<Price>(), price, delta, idx, pool);
    } else {
      removed = Reduce(AskIndices, std::less<Price>(), price, delta, idx, pool);
    }
    std::uint64_t t1 = core::rdtsc();
    reduce_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
    return removed;
  }

  std::tuple<Price, Shares, Price, Shares> top_of_book() const {
    Price bid_px = 0;
    Shares bid_sz = 0;
    Price ask_px = 0;
    Shares ask_sz = 0;

    if (!BidIndices.empty()) {
        const PriceLevel& lvl = level_pool[BidIndices.front()];
        bid_px = lvl.price;
        bid_sz = static_cast<Shares>(lvl.volume);
    }
    if (!AskIndices.empty()) {
        const PriceLevel& lvl = level_pool[AskIndices.front()];
        ask_px = lvl.price;
        ask_sz = static_cast<Shares>(lvl.volume);
    }
    return {bid_px, bid_sz, ask_px, ask_sz};
}

};

} // namespace lowlat::book