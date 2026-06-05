#include <thread>
#include <vector>
#include <atomic>
#include <pthread.h>
#include <sched.h>
#include <gtest/gtest.h>
#include <lowlat/queue/spsc_queue.hpp>

static void pin(std::thread& t, int core) {
    cpu_set_t s; CPU_ZERO(&s); CPU_SET(core, &s);
    pthread_setaffinity_np(t.native_handle(), sizeof(s), &s);
}

TEST(SPSCQueue, SingleThreadedBasic) {
    lowlat::queue::SPSCQueue<int, 8> q;
    int out;

    EXPECT_FALSE(q.try_pop(out));
    EXPECT_TRUE(q.try_push(42));
    q.flush_writes();
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 42);
    EXPECT_FALSE(q.try_pop(out));
}

TEST(SPSCQueue, FillAndDrain) {
    lowlat::queue::SPSCQueue<int, 4> q;

    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    EXPECT_TRUE(q.try_push(4));
    EXPECT_FALSE(q.try_push(5));  // full
    q.flush_writes();

    int out;
    EXPECT_TRUE(q.try_pop(out));  EXPECT_EQ(out, 1);
    EXPECT_TRUE(q.try_pop(out));  EXPECT_EQ(out, 2);
    EXPECT_TRUE(q.try_pop(out));  EXPECT_EQ(out, 3);
    EXPECT_TRUE(q.try_pop(out));  EXPECT_EQ(out, 4);
    EXPECT_FALSE(q.try_pop(out)); // empty
    q.flush_reads();
}

TEST(SPSCQueue, ProducerConsumerInOrder) {
    constexpr std::size_t CAP = 1 << 14; // must exceed BATCH_SIZE (8192)
    constexpr std::uint64_t N = 1'000'000;

    lowlat::queue::SPSCQueue<std::uint64_t, CAP> q;

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < N; ++i)
            while (!q.try_push(i)) {}
        q.flush_writes();
    });
    pin(producer, 2);

    std::uint64_t expected = 0, val;
    while (expected < N) {
        if (q.try_pop(val)) {
            ASSERT_EQ(val, expected);
            ++expected;
        }
    }
    q.flush_reads();
    producer.join();
    EXPECT_EQ(expected, N);
}