#pragma once

#include <atomic>
#include <array>
#include <cstdint>

namespace lowlat::queue {
    
inline constexpr std::uint32_t CACHE_LINE_SIZE = 64;

template<typename T, std::size_t Capacity>
struct SPSCQueue{

    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static constexpr std::size_t MASK = Capacity - 1;

    alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> mReadCounter{0};
    alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> mWriteCounter{0};
    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer;
    std::uint64_t reader_cnt{0}; // local cnt for try_pop
    std::uint64_t writer_cnt{0}; // local cnt for try_push

    bool try_push(const T& item) {

        std::uint64_t curr_read = mReadCounter.load(std::memory_order_acquire);
        if (writer_cnt - curr_read >= Capacity) return false; // queue was full
        buffer[(writer_cnt & MASK)] = item;
        writer_cnt ++;
        mWriteCounter.store(writer_cnt, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) {

        std::uint64_t curr_write = mWriteCounter.load(std::memory_order_acquire);
        if (curr_write == reader_cnt) return false; // empty
        out = buffer[(reader_cnt & MASK)];
        reader_cnt++;
        mReadCounter.store(reader_cnt, std::memory_order_release);
        return true;
    }

};

} // namespace lowlat::queue

