#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace lowlat::queue {

inline constexpr std::uint32_t CACHE_LINE_SIZE = 64;

template <typename T, std::size_t Capacity> struct SPSCQueue {

  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");
  static constexpr std::size_t MASK = Capacity - 1;
  static constexpr std::uint32_t BATCH_SIZE = 8192;

  alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> mReadCounter{0};
  alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> mWriteCounter{0};
  alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer;
  std::uint64_t reader_cnt{0};   // local cnt for try_pop
  std::uint64_t writer_cnt{0};   // local cnt for try_push
  std::uint64_t cached_read{0};  // cached_read for try_push
  std::uint64_t cached_write{0}; // cached_write for try_pop

  std::uint64_t pub_write{0}; // last write published by try_push
  std::uint64_t pub_read{0};

  bool try_push(const T &item) {

    if (writer_cnt - cached_read >= Capacity) {
      // check again
      cached_read = mReadCounter.load(std::memory_order_acquire);
      if (writer_cnt - cached_read >= Capacity) {
        return false; // queue is full
      }
    }

    buffer[(writer_cnt & MASK)] = item;
    writer_cnt++;
    if (writer_cnt - pub_write >= BATCH_SIZE) {
      pub_write = writer_cnt;
      mWriteCounter.store(writer_cnt, std::memory_order_release);
    }
    return true;
  }

  bool try_pop(T &out) {
    if (reader_cnt == cached_write) {
      cached_write = mWriteCounter.load(std::memory_order_acquire);
      if (cached_write == reader_cnt)
        return false; // empty
    }
    out = buffer[(reader_cnt & MASK)];
    reader_cnt++;
    if (reader_cnt - pub_read >= BATCH_SIZE) {
      pub_read = reader_cnt;
      mReadCounter.store(reader_cnt, std::memory_order_release);
    }
    return true;
  }

  void flush_writes() {
    if (writer_cnt != pub_write) {
      mWriteCounter.store(writer_cnt, std::memory_order_release);
      pub_write = writer_cnt;
    }
  }

  void flush_reads() {
    if (reader_cnt != pub_read) {
      mReadCounter.store(reader_cnt, std::memory_order_release);
      pub_read = reader_cnt;
    }
  }
};

} // namespace lowlat::queue
