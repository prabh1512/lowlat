#pragma once

#include <map>
#include <unordered_map>
#include <stdexcept>
#include <lowlat/book/order.hpp>
#include <lowlat/book/order_pool.hpp>
#include <utility>
#include <lowlat/core/tsc.hpp>
#include <vector>

namespace lowlat::book {

using Volume = std::uint64_t;

struct CommodityBook {
    static inline std::vector<std::uint32_t> add_cycles;
    static inline std::vector<std::uint32_t> reduce_cycles;
    std::map<Price, Volume, std::greater<Price>> BidLevels;
    std::map<Price, Volume, std::less<Price>>    AskLevels;
    std::unordered_map<Price, std::pair<uint32_t, uint32_t>> BidHT; //um
    std::unordered_map<Price, std::pair<uint32_t, uint32_t>> AskHT; //um

    template <typename Levels, typename HT>
    void Add(Levels& levels, HT& ht, Price price, Shares shares, std::uint32_t idx, OrderPool& pool) {
        if (ht.find(price) == ht.end()) ht[price] = std::make_pair(NIL, NIL);
        auto& [u, v] = ht[price];
        if (v != NIL) pool.pool[v].next = idx;
        pool.pool[idx].prev = v;
        pool.pool[idx].next = NIL;
        v = idx;
        if (u == NIL) u = idx;
        auto [it, inserted] = levels.try_emplace(price, shares);
        if (!inserted) it->second += shares;
    }

    template <typename Levels, typename HT>
    bool Reduce(Levels& levels, HT& ht, Price price, Shares delta, std::uint32_t idx, OrderPool& pool) {
        Order& order = pool.pool[idx];

        if (delta > order.shares) {
            throw std::runtime_error("reduce: delta exceeds order shares");
        }

        order.shares -= delta;

        auto lvl_it = levels.find(price);
        lvl_it->second -= delta;

        if (order.shares == 0) {
            auto& [head, tail] = ht[price];
            if (order.prev != NIL) pool.pool[order.prev].next = order.next;
            else head = order.next;
            if (order.next != NIL) pool.pool[order.next].prev = order.prev;
            else tail = order.prev;

            if (lvl_it->second == 0) {
                levels.erase(lvl_it);
                ht.erase(price);
            }

            return true;
        }
        return false;
    }

template <Side S>
void Add(Order order, std::uint32_t idx, OrderPool& pool) {
    std::uint64_t t0 = core::rdtsc();
    if constexpr (S == Side::Bid) {
        Add(BidLevels, BidHT, order.price, order.shares, idx, pool);
    } else {
        Add(AskLevels, AskHT, order.price, order.shares, idx, pool);
    }
    std::uint64_t t1 = core::rdtsc();
    add_cycles.push_back(static_cast<std::uint32_t>(t1 - t0));
}

template <Side S>
bool Reduce(Price price, Shares delta, std::uint32_t idx, OrderPool& pool) {
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

};

}  // namespace lowlat::book