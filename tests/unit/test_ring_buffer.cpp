#include <gtest/gtest.h>
#include "pulseport/ring_buffer.h"

using namespace pulseport;

TEST(RingBuffer, PushAndRead) {
    RingBuffer<int, 4> rb;  // capacity=4

    EXPECT_TRUE(rb.push(10));
    EXPECT_TRUE(rb.push(20));
    EXPECT_TRUE(rb.push(30));

    auto latest = rb.latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(*latest, 30);
}

TEST(RingBuffer, RecentReturnsInOrder) {
    RingBuffer<int, 8> rb;

    for (int i = 1; i <= 5; ++i) {
        rb.push(i);
    }

    auto recent = rb.recent(3);
    ASSERT_EQ(recent.size(), 3u);
    // Most recent first
    EXPECT_EQ(recent[0], 5);
    EXPECT_EQ(recent[1], 4);
    EXPECT_EQ(recent[2], 3);
}

TEST(RingBuffer, WrapsAroundCorrectly) {
    RingBuffer<int, 4> rb;

    // Write more than capacity
    for (int i = 1; i <= 10; ++i) {
        rb.push(i);
    }

    auto latest = rb.latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(*latest, 10);

    // Should have the last 4 values
    auto recent = rb.recent(4);
    ASSERT_EQ(recent.size(), 4u);
    EXPECT_EQ(recent[0], 10);
    EXPECT_EQ(recent[1], 9);
    EXPECT_EQ(recent[2], 8);
    EXPECT_EQ(recent[3], 7);
}

TEST(RingBuffer, EmptyBufferReturnsNone) {
    RingBuffer<int, 4> rb;

    auto latest = rb.latest();
    EXPECT_FALSE(latest.has_value());

    auto recent = rb.recent(5);
    EXPECT_TRUE(recent.empty());
}

TEST(RingBuffer, SnapshotCopiesAll) {
    RingBuffer<int, 8> rb;
    for (int i = 0; i < 5; ++i) {
        rb.push(i * 10);
    }

    auto snap = rb.snapshot();
    ASSERT_EQ(snap.size(), 5u);
}

TEST(RingBuffer, MetricSampleType) {
    RingBuffer<MetricSample, 4> rb;

    MetricSample s;
    s.key = "cpu.total_pct";
    s.value = 42.5;
    s.unit = "%";
    s.quality = Quality::Measured;
    s.timestamp = std::chrono::system_clock::now();

    rb.push(s);

    auto latest = rb.latest();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->key, "cpu.total_pct");
    EXPECT_DOUBLE_EQ(latest->value, 42.5);
    EXPECT_EQ(latest->quality, Quality::Measured);
}
