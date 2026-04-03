#include <gtest/gtest.h>
#include "pulseport/storage.h"
#include "pulseport/database.h"

#include <filesystem>

using namespace pulseport;

class RetentionCleanupTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = std::filesystem::temp_directory_path() / "retention_test.db";
        std::filesystem::remove(db_path_);
        db_.open(db_path_, "");
        writer_ = std::make_unique<StorageWriter>(db_.handle());
        reader_ = std::make_unique<StorageReader>(db_.handle());
    }

    void TearDown() override {
        writer_.reset();
        reader_.reset();
        db_.close();
        std::filesystem::remove(db_path_);
    }

    std::filesystem::path db_path_;
    Database db_;
    std::unique_ptr<StorageWriter> writer_;
    std::unique_ptr<StorageReader> reader_;
};

TEST_F(RetentionCleanupTest, CleanupRemovesOldEvents) {
    int64_t ts_now = now_unix();
    int64_t ts_old = ts_now - (100 * 86400); // 100 days ago

    // Write an old event and a recent event
    writer_->write_event(ts_old, "info", "test", "Old event");
    writer_->write_event(ts_now, "info", "test", "Recent event");

    StorageReader::RetentionConfig rc;
    rc.retention_events_days = 30; // Keep only 30 days
    rc.retention_1m_days = 30;
    rc.retention_15m_days = 90;
    rc.retention_daily_days = 365;
    reader_->cleanup_expired(rc);

    auto events = reader_->query_events(0, ts_now + 60, "test");
    EXPECT_EQ(events.size(), 1u);
    if (!events.empty()) {
        EXPECT_EQ(events[0].title, "Recent event");
    }
}

TEST_F(RetentionCleanupTest, CleanupWithNoDataDoesNotCrash) {
    StorageReader::RetentionConfig rc;
    rc.retention_1m_days = 7;
    rc.retention_15m_days = 30;
    rc.retention_daily_days = 365;
    rc.retention_events_days = 30;

    // Should not crash on empty database
    reader_->cleanup_expired(rc);
    SUCCEED();
}
