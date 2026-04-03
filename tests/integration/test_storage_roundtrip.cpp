#include <gtest/gtest.h>
#include "pulseport/database.h"
#include "pulseport/storage.h"
#include "pulseport/types.h"
#include <filesystem>

using namespace pulseport;

class StorageRoundtripTest : public ::testing::Test {
protected:
    std::filesystem::path temp_dir;
    std::filesystem::path db_path;
    std::filesystem::path migrations_path;
    Database db;

    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / "pulseport_test_storage";
        std::filesystem::create_directories(temp_dir);
        db_path = temp_dir / "test.db";
        migrations_path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "db" / "migrations";

        ASSERT_TRUE(db.open(db_path.string(), migrations_path.string()));
    }

    void TearDown() override {
        db.close();
        std::filesystem::remove_all(temp_dir);
    }
};

TEST_F(StorageRoundtripTest, WriteAndReadMetric1m) {
    StorageWriter writer(db);
    StorageReader reader(db);

    MetricAggregate agg;
    agg.key = "test.cpu";
    agg.bucket_ts = 1700000000;
    agg.min_val = 10.0;
    agg.max_val = 90.0;
    agg.avg_val = 50.0;
    agg.count = 60;

    writer.write_1m({agg});

    auto results = reader.query_history("test.cpu", "metric_1m", 1699999900, 1700000100);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].key, "test.cpu");
    EXPECT_DOUBLE_EQ(results[0].avg_val, 50.0);
    EXPECT_DOUBLE_EQ(results[0].min_val, 10.0);
    EXPECT_DOUBLE_EQ(results[0].max_val, 90.0);
    EXPECT_EQ(results[0].count, 60u);
}

TEST_F(StorageRoundtripTest, WriteAndReadEnergyDaily) {
    StorageWriter writer(db);
    StorageReader reader(db);

    writer.write_energy_daily("2024-01-15", 1234.5, 51.4);

    auto results = reader.query_energy_daily("2024-01-01", "2024-01-31");
    ASSERT_GE(results.size(), 1u);
}

TEST_F(StorageRoundtripTest, WriteAndReadEvents) {
    StorageWriter writer(db);
    StorageReader reader(db);

    int64_t now = 1700000000;
    writer.write_event(now, "info", "lifecycle", "Service started");

    auto events = reader.query_events(now - 100, now + 100);
    ASSERT_GE(events.size(), 1u);
}

TEST_F(StorageRoundtripTest, DeleteWithAudit) {
    StorageWriter writer(db);
    StorageReader reader(db);

    // Insert data
    MetricAggregate agg;
    agg.key = "delete.test";
    agg.bucket_ts = 1700000000;
    agg.min_val = 1.0;
    agg.max_val = 1.0;
    agg.avg_val = 1.0;
    agg.count = 1;
    writer.write_1m({agg});

    // Delete it
    int deleted = reader.delete_history("metric_1m", 1699999900, 1700000100);
    EXPECT_EQ(deleted, 1);

    // Verify it's gone
    auto results = reader.query_history("delete.test", "metric_1m", 1699999900, 1700000100);
    EXPECT_TRUE(results.empty());
}
