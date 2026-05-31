#pragma once

#include <vector>
#include <unordered_map>
#include <lowlat/book/order.hpp>
#include <lowlat/core/tsc.hpp>
#include <utility>

namespace lowlat::book {

std::vector<std::pair<Price, Shares>> BidLevels;
std::vector<std::pair<Price, Shares>> AskLevels;

std::unordered_map<Price, std::pair<uint32_t, uint32_t>> BidHT;
std::unordered_map<Price, std::pair<uint32_t, uint32_t>> AskHT;


} // namespace lowlat::book
