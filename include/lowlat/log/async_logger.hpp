#pragma once

#include <atomic>
#include <cstdio>
#include <lowlat/book/book_update.hpp>
#include <lowlat/queue/spsc_queue.hpp>
#include <pthread.h>
#include <sched.h>
#include <thread>

namespace lowlat::log {

inline constexpr std::size_t LOG_QUEUE_CAP = 1 << 16; // 2^16, power of 2

class AsyncLogger {
public:
  using Queue = lowlat::queue::SPSCQueue<book::BookUpdate, LOG_QUEUE_CAP>;

  explicit AsyncLogger(const std::string &path)
      : file_(std::fopen(path.c_str(), "wb")),
        worker_(&AsyncLogger::drain_loop, this) {
    std::setvbuf(file_, nullptr, _IOFBF, 1 << 20); // 1MB user buffer
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(6, &set); // logger on core 6
    pthread_setaffinity_np(worker_.native_handle(), sizeof(set), &set);
  }

  ~AsyncLogger() {
    stop_.store(true, std::memory_order_release);
    if (worker_.joinable())
      worker_.join();
    if (file_)
      std::fclose(file_);
  }

  Queue &queue() { return queue_; }

private:
  void drain_loop() {
    book::BookUpdate u;
    while (!stop_.load(std::memory_order_acquire)) {
      if (!queue_.try_pop(u)) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        continue;
      }
      std::fwrite(&u, sizeof(u), 1, file_);
      // drain a batch before checking stop_
      while (queue_.try_pop(u)) {
        std::fwrite(&u, sizeof(u), 1, file_);
      }
    }
    while (queue_.try_pop(u)) {
      std::fwrite(&u, sizeof(u), 1, file_);
    }
  }
  Queue queue_{};
  std::FILE *file_;
  std::atomic<bool> stop_{false};
  std::thread worker_;
};

} // namespace lowlat::log