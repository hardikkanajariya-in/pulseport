#include <gtest/gtest.h>
#include "pulseport/ring_buffer.h"

#include <chrono>
#include <vector>

using namespace pulseport;

TEST(RingBufferPerf, ThroughputMillionPushes) {
    RingBuffer<double, 1024> rb;

    auto start = std::chrono::high_resolution_clock::now();

    constexpr int kIterations = 1'000'000;
    for (int i = 0; i < kIterations; ++i) {
        rb.push(static_cast<double>(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Ring buffer should handle > 1M pushes well under 100ms
    EXPECT_LT(ms, 100) << "1M pushes took " << ms << "ms — too slow";

    // Verify last value is correct
    auto latest = rb.latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_DOUBLE_EQ(*latest, kIterations - 1.0);
}

TEST(RingBufferPerf, RecentAccessPerformance) {
    RingBuffer<double, 2048> rb;

    // Fill buffer
    for (int i = 0; i < 2048; ++i) {
        rb.push(static_cast<double>(i));
    }

    auto start = std::chrono::high_resolution_clock::now();

    constexpr int kReads = 100'000;
    for (int i = 0; i < kReads; ++i) {
        auto recent = rb.recent(512);
        ASSERT_EQ(recent.size(), 512u);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 100K reads of 512 elements should be under 500ms
    EXPECT_LT(ms, 500) << "100K recent(512) took " << ms << "ms";
}
