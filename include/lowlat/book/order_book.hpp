#pragma once

#include <unordered_map>
#include <stdexcept>
#include <lowlat/book/order.hpp>
#include <lowlat/book/order_pool.hpp>
#include <lowlat/book/commodity_book_5.hpp>

namespace lowlat::book {


inline constexpr std::uint32_t MAX_STOCKS = 10'000;

template <typename CB>
struct OrderBook {
    OrderPool order_pool;
    std::unordered_map<OrderId, std::uint32_t> id_to_pool; //um
    // std::unordered_map<Stock, CB> stock_to_book; //um
    std::unique_ptr<std::array<CB, MAX_STOCKS>> stock_to_book = std::make_unique<std::array<CB, MAX_STOCKS>>();
    std::uint32_t peak_live_orders = 0;

    void AddOrder(const Order& order) {
        std::uint32_t idx = order_pool.InsertOrder(order);
        id_to_pool[order.order_id] = idx;
        if (id_to_pool.size() > peak_live_orders) peak_live_orders = static_cast<std::uint32_t>(id_to_pool.size());
        auto& cb = (*stock_to_book)[order.stock];

        if (order.side == Side::Bid) {
            cb.template Add<Side::Bid>(order, idx, order_pool);
        } else {
            cb.template Add<Side::Ask>(order, idx, order_pool);
        }
    }

    // Reduces an order by `delta` shares. If shares hit zero, the order is fully removed.
    // Used for Execute and partial Cancel. Pass delta == order.shares for full Delete.
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

    // Full delete = reduce by full remaining shares.
    void DeleteOrder(OrderId id) {
        auto it = id_to_pool.find(id);
        if (it == id_to_pool.end()) [[unlikely]] {
            throw std::runtime_error("DeleteOrder: unknown order_id");
        }
        Shares remaining = order_pool.pool[it->second].shares;
        ReduceOrder(id, remaining);
    }

    // Atomic cancel-old + add-new at new price/shares (same stock + side as old).
    void ReplaceOrder(OrderId old_id, OrderId new_id, Price new_price, Shares new_shares) {
        auto it = id_to_pool.find(old_id);
        if (it == id_to_pool.end()) [[unlikely]] {
            throw std::runtime_error("ReplaceOrder: unknown old order_id");
        }
        Order& old_order = order_pool.pool[it->second];
        Stock stock = old_order.stock;
        Side  side  = old_order.side;

        DeleteOrder(old_id);

        Order new_order;
        new_order.stock   = stock;
        new_order.order_id = new_id;
        new_order.shares  = new_shares;
        new_order.price   = new_price;
        new_order.side    = side;
        new_order.prev    = NIL;
        new_order.next    = NIL;

        AddOrder(new_order);
    }
};

}  // namespace