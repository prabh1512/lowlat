#pragma once

#include <cstdint>
#include <x86intrin.h>

namespace lowlat::core {

[[nodiscard]] inline std::uint64_t rdtsc() noexcept {
    unsigned aux;
    return __rdtscp(&aux);
}

}  // namespace lowlat::core