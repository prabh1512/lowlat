#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <lowlat/queue/spsc_queue.hpp>

namespace {
void pin_to_core(std::size_t core) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}
}  // namespace

int main() {
    constexpr std::size_t CAP = 1 << 14;
    constexpr std::uint64_t WARMUP_N = 10'000'000;
    constexpr std::uint64_t N = 1'000'000'000;

    using Q = lowlat::queue::SPSCQueue<std::uint64_t, CAP>;
    auto q = std::make_unique<Q>();

    // Warmup
    std::thread wp([&] {
        pin_to_core(2);
        for (std::uint64_t i = 0; i < WARMUP_N; ++i) {
            while (!q->try_push(i)) {}
        }
        q->flush_writes();
    });
    std::thread wc([&] {
        pin_to_core(4);
        std::uint64_t v;
        for (std::uint64_t i = 0; i < WARMUP_N; ++i) {
            while (!q->try_pop(v)) {}
        }
        q->flush_reads();
    });
    wp.join();
    wc.join();

    // Measure
    auto t0 = std::chrono::steady_clock::now();

    std::thread producer([&] {
        pin_to_core(2);
        for (std::uint64_t i = 0; i < N; ++i) {
            while (!q->try_push(i)) {}
        }
        q->flush_writes();
    });
    std::thread consumer([&] {
        pin_to_core(4);
        std::uint64_t v;
        for (std::uint64_t i = 0; i < N; ++i) {
            while (!q->try_pop(v)) {}
        }
        q->flush_reads();
    });
    producer.join();
    consumer.join();

    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    double per_op_ns = secs * 1e9 / N;

    std::cout << "SPSC throughput benchmark\n";
    std::cout << "items:      " << N << '\n';
    std::cout << "wall (s):   " << secs << '\n';
    std::cout << "throughput: " << static_cast<std::uint64_t>(N / secs) << " items/sec\n";
    std::cout << "per-op:     " << per_op_ns << " ns (avg)\n";
    return 0;
}