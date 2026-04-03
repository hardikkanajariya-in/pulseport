#include <gtest/gtest.h>
#include "pulseport/types.h"

using namespace pulseport;

TEST(Types, QualityToString) {
    EXPECT_EQ(quality_to_string(Quality::Measured), "measured");
    EXPECT_EQ(quality_to_string(Quality::Derived), "derived");
    EXPECT_EQ(quality_to_string(Quality::Estimated), "estimated");
    EXPECT_EQ(quality_to_string(Quality::Unknown), "unknown");
}

TEST(Types, ToUnixTimestamp) {
    auto now = std::chrono::system_clock::now();
    int64_t unix_ts = to_unix(now);

    // Should be a reasonable current timestamp (> 2024-01-01)
    EXPECT_GT(unix_ts, 1704067200);
}

TEST(Types, FromUnixRoundTrip) {
    auto now = std::chrono::system_clock::now();
    int64_t unix_ts = to_unix(now);
    auto restored = from_unix(unix_ts);

    // Should be within 1 second of original (truncation to seconds)
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - restored).count();
    EXPECT_LE(std::abs(diff), 1);
}

TEST(Types, MetricSampleConstruction) {
    MetricSample s;
    s.key = "test.metric";
    s.value = 3.14;
    s.unit = "units";
    s.quality = Quality::Derived;
    s.timestamp = std::chrono::system_clock::now();

    EXPECT_EQ(s.key, "test.metric");
    EXPECT_DOUBLE_EQ(s.value, 3.14);
    EXPECT_EQ(s.quality, Quality::Derived);
}

TEST(Types, MetricAggregateAccumulation) {
    MetricAggregate agg;
    agg.key = "cpu.total_pct";
    agg.bucket_ts = 1700000000;
    agg.min_val = 10.0;
    agg.max_val = 90.0;
    agg.avg_val = 50.0;
    agg.count = 60;

    EXPECT_EQ(agg.count, 60u);
    EXPECT_LE(agg.min_val, agg.avg_val);
    EXPECT_GE(agg.max_val, agg.avg_val);
}
