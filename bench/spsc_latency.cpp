#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <random>
#include <sched.h>
#include <thread>
#include <unordered_set>
#include <vector>
#include <lowlat/queue/spsc_queue.hpp>
#include <lowlat/core/tsc.hpp>

namespace {
void pin_to_core(std::size_t core) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

struct Message {
    std::uint64_t timestamp;  // 0 = not sampled; nonzero = producer's TSC at push
    std::uint64_t data;
};
}  // namespace

int main() {
    using lowlat::core::rdtsc;

    constexpr std::size_t CAP = 1 << 14;
    constexpr std::uint64_t WARMUP_N = 10'000'000;
    constexpr std::uint64_t N = 1'000'000'000;
    constexpr std::size_t NUM_SAMPLES = 100'000;
    constexpr double CYCLES_PER_NS = 2.304;

    using Q = lowlat::queue::SPSCQueue<Message, CAP>;
    auto q = std::make_unique<Q>();

    // Sample indices: NUM_SAMPLES random points across N pushes
    std::vector<std::uint64_t> sample_indices;
    {
        std::mt19937_64 gen(42);
        std::uniform_int_distribution<std::uint64_t> dist(0, N - 1);
        std::unordered_set<std::uint64_t> s;
        while (s.size() < NUM_SAMPLES) s.insert(dist(gen));
        sample_indices.assign(s.begin(), s.end());
        std::sort(sample_indices.begin(), sample_indices.end());
    }

    std::vector<std::uint64_t> latencies;
    latencies.reserve(NUM_SAMPLES);

    // Warmup (no sampling)
    std::thread wp([&] {
        pin_to_core(2);
        for (std::uint64_t i = 0; i < WARMUP_N; ++i) {
            while (!q->try_push({0, i})) {}
        }
        q->flush_writes();
    });
    std::thread wc([&] {
        pin_to_core(4);
        Message v;
        for (std::uint64_t i = 0; i < WARMUP_N; ++i) {
            while (!q->try_pop(v)) {}
        }
        q->flush_reads();
    });
    wp.join();
    wc.join();

    // Measure
    std::cout << "starting latency benchmark, " << NUM_SAMPLES << " samples in " << N << " pushes\n";

    std::thread producer([&] {
        pin_to_core(2);
        std::size_t next = 0;
        for (std::uint64_t i = 0; i < N; ++i) {
            std::uint64_t ts = 0;
            if (next < sample_indices.size() && i == sample_indices[next]) {
                ts = rdtsc();
                ++next;
            }
            while (!q->try_push({ts, i})) {}
        }
        q->flush_writes();
    });

    std::thread consumer([&] {
        pin_to_core(4);
        Message m;
        for (std::uint64_t i = 0; i < N; ++i) {
            while (!q->try_pop(m)) {}
            if (m.timestamp != 0) {
                std::uint64_t now = rdtsc();
                latencies.push_back(now - m.timestamp);
            }
        }
        q->flush_reads();
    });

    producer.join();
    consumer.join();

    std::sort(latencies.begin(), latencies.end());

    auto pct = [&](double p) {
        if (latencies.empty()) return std::uint64_t{0};
        std::size_t idx = static_cast<std::size_t>(p * static_cast<double>(latencies.size() - 1) / 100.0);
        return latencies[idx];
    };
    auto ns = [](std::uint64_t c) {
        return static_cast<std::uint32_t>(static_cast<double>(c) / CYCLES_PER_NS);
    };

    std::cout << "samples collected: " << latencies.size() << '\n';
    std::cout << "end-to-end latency (push to pop, ns):\n";
    std::cout << "  p50:    " << ns(pct(50)) << '\n';
    std::cout << "  p90:    " << ns(pct(90)) << '\n';
    std::cout << "  p99:    " << ns(pct(99)) << '\n';
    std::cout << "  p99.9:  " << ns(pct(99.9)) << '\n';
    std::cout << "  p99.99: " << ns(pct(99.99)) << '\n';
    std::cout << "  max:    " << ns(latencies.back()) << '\n';
    return 0;
}