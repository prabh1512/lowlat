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

    std::uint32_t InsertOrder(const Order& order){
        if (free_list_head == NIL){
            throw std::runtime_error("pool is full");
        }

        uint32_t new_head = pool[(free_list_head)].next;
        pool[free_list_head] = order;
        uint32_t res = free_list_head;
        free_list_head = new_head;
        return res;
    }

    void DeleteOrder(std::uint32_t idx){
        std::uint32_t old_head = free_list_head;
        pool[idx].next = old_head;
        free_list_head = idx;
    }

};

} // namespace lowlat::book