#pragma once

#include <cstdint>
#include <lowlat/book/order.hpp>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <utility>

namespace lowlat::book {

inline constexpr uint32_t MAX_ORDERS = 1'000'000;

struct OrderPool{
    std::uint32_t free_list_head;
    std::vector<Order> pool;

    OrderPool(){
        free_list_head = 0;
        pool.resize(MAX_ORDERS);
        for (std::uint32_t i = 0; i < MAX_ORDERS; i++){
            pool[i].next = (i+1 < MAX_ORDERS ? i+1 : NIL);
        }
    }

    std::uint32_t Allocate() {
        if (free_list_head == NIL) [[unlikely]] {
            throw std::runtime_error("pool is full");
        }
        std::uint32_t idx = free_list_head;
        free_list_head = pool[idx].next;
        return idx;
    }

    void DeleteOrder(std::uint32_t idx){
        std::uint32_t old_head = free_list_head;
        pool[idx].next = old_head;
        free_list_head = idx;
    }

};

} // namespace lowlat::book