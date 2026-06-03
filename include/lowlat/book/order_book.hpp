#pragma once

#include <unordered_map>
#include <array>
#include <memory>
#include <stdexcept>
#include <lowlat/book/order.hpp>
#include <lowlat/book/order_pool.hpp>
#include <lowlat/book/commodity_book_5.hpp>
#include <absl/container/flat_hash_map.h>

namespace lowlat::book {

inline constexpr std::uint32_t MAX_STOCKS = 10000;

template <typename CB, typename IdMap = std::unordered_map<OrderId, std::uint32_t>>
struct OrderBook {
    OrderPool order_pool;
    IdMap id_to_pool;
    std::unique_ptr<std::array<CB, MAX_STOCKS>> stock_to_book =
        std::make_unique<std::array<CB, MAX_STOCKS>>();
    std::uint32_t peak_live_orders = 0;

    void AddOrder(Stock stock, OrderId id, Shares shares, Price price, Side side) {
        std::uint32_t idx = order_pool.Allocate();
        Order& slot = order_pool.pool[idx];
        slot.stock    = stock;
        slot.order_id = id;
        slot.shares   = shares;
        slot.price    = price;
        slot.side     = side;

        id_to_pool[id] = idx;
        if (id_to_pool.size() > peak_live_orders)
            peak_live_orders = static_cast<std::uint32_t>(id_to_pool.size());

        auto& cb = (*stock_to_book)[stock];
        if (side == Side::Bid) {
            cb.template Add<Side::Bid>(price, shares, idx, order_pool);
        } else {
            cb.template Add<Side::Ask>(price, shares, idx, order_pool);
        }
    }

    void ReduceOrder(OrderId id, Shares delta) {
        auto it = id_to_pool.find(id);
        if (it == id_to_pool.end()) [[unlikely]] {
            throw std::runtime_error("ReduceOrder: unknown order_id");
        }
        std::uint32_t idx = it->second;
        Order& order = order_pool.pool[idx];
        Stock  stock = order.stock;
        Price  price = order.price;
        Side   side  = order.side;

        auto& cb = (*stock_to_book)[stock];

        bool removed;
        if (side == Side::Bid) {
            removed = cb.template Reduce<Side::Bid>(price, delta, idx, order_pool);
        } else {
            removed = cb.template Reduce<Side::Ask>(price, delta, idx, order_pool);
        }

        if (removed) {
            id_to_pool.erase(it);
            order_pool.DeleteOrder(idx);
        }
    }

    void DeleteOrder(OrderId id) {
        auto it = id_to_pool.find(id);
        if (it == id_to_pool.end()) [[unlikely]] {
            throw std::runtime_error("DeleteOrder: unknown order_id");
        }
        Shares remaining = order_pool.pool[it->second].shares;
        ReduceOrder(id, remaining);
    }

    void ReplaceOrder(OrderId old_id, OrderId new_id, Price new_price, Shares new_shares) {
        auto it = id_to_pool.find(old_id);
        if (it == id_to_pool.end()) [[unlikely]] {
            throw std::runtime_error("ReplaceOrder: unknown old order_id");
        }
        Order& old_order = order_pool.pool[it->second];
        Stock stock = old_order.stock;
        Side  side  = old_order.side;

        DeleteOrder(old_id);
        AddOrder(stock, new_id, new_shares, new_price, side);
    }
};

}  // namespace lowlat::book