#include <gtest/gtest.h>
#include "pulseport/storage.h"
#include "pulseport/database.h"

#include <chrono>
#include <filesystem>

using namespace pulseport;

class SqliteWritePerf : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = std::filesystem::temp_directory_path() / "sqlite_perf_test.db";
        std::filesystem::remove(db_path_);
        db_.open(db_path_, "");
        writer_ = std::make_unique<StorageWriter>(db_.handle());
    }

    void TearDown() override {
        writer_.reset();
        db_.close();
        std::filesystem::remove(db_path_);
    }

    std::filesystem::path db_path_;
    Database db_;
    std::unique_ptr<StorageWriter> writer_;
};

TEST_F(SqliteWritePerf, EventWriteThroughput) {
    constexpr int kEvents = 10'000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < kEvents; ++i) {
        writer_->write_event(
            now_unix() + i, "info", "perf_test",
            "Test event " + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double events_per_sec = kEvents * 1000.0 / std::max(ms, 1LL);

    // WAL mode should handle > 1000 event writes/sec easily
    EXPECT_GT(events_per_sec, 1000.0)
        << "Event write throughput: " << events_per_sec << " events/s (" << ms << "ms for " << kEvents << ")";
}

TEST_F(SqliteWritePerf, AggregateWriteThroughput) {
    constexpr int kAggregates = 5'000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < kAggregates; ++i) {
        MetricAggregate agg;
        agg.key = "cpu.total_pct";
        agg.bucket_ts = now_unix() - (kAggregates - i) * 60;
        agg.min_value = 10.0;
        agg.max_value = 90.0;
        agg.avg_value = 50.0;
        agg.sample_count = 60;
        agg.quality = Quality::Measured;
        writer_->write_aggregate_1m(agg);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double writes_per_sec = kAggregates * 1000.0 / std::max(ms, 1LL);
    EXPECT_GT(writes_per_sec, 500.0)
        << "Aggregate write throughput: " << writes_per_sec << " writes/s";
}
