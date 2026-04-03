#include <gtest/gtest.h>
#include "pulseport/alert_evaluator.h"
#include "pulseport/metric_registry.h"
#include "pulseport/storage.h"
#include "pulseport/database.h"

#include <filesystem>

using namespace pulseport;

class AlertEvaluatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = std::filesystem::temp_directory_path() / "alert_test.db";
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

TEST_F(AlertEvaluatorTest, NoAlertBelowThreshold) {
    MetricRegistry registry;
    registry.register_metric({"cpu.total_pct", "CPU Total", "%", "pdh", "cpu"});

    AlertEvaluator::Thresholds thresholds;
    thresholds.cpu_high_pct = 90.0;
    thresholds.cpu_sustained_min = 3;
    AlertEvaluator evaluator(*writer_, thresholds);

    registry.push_sample({"cpu.total_pct", 50.0, "%", Quality::Measured, now_unix()});
    evaluator.evaluate(registry);

    // No events should be written
    auto events = reader_->query_events(0, now_unix() + 60, "alert");
    EXPECT_EQ(events.size(), 0u);
}

TEST_F(AlertEvaluatorTest, AlertAfterSustainedViolation) {
    MetricRegistry registry;
    registry.register_metric({"cpu.total_pct", "CPU Total", "%", "pdh", "cpu"});

    AlertEvaluator::Thresholds thresholds;
    thresholds.cpu_high_pct = 90.0;
    thresholds.cpu_sustained_min = 2;
    thresholds.cooldown_minutes = 0;
    AlertEvaluator evaluator(*writer_, thresholds);

    registry.push_sample({"cpu.total_pct", 95.0, "%", Quality::Measured, now_unix()});

    evaluator.evaluate(registry);
    evaluator.evaluate(registry);
    evaluator.evaluate(registry);

    auto events = reader_->query_events(0, now_unix() + 60, "alert");
    EXPECT_GE(events.size(), 1u);
}

TEST_F(AlertEvaluatorTest, ThresholdCanBeUpdated) {
    AlertEvaluator::Thresholds t1;
    t1.cpu_high_pct = 90.0;
    AlertEvaluator evaluator(*writer_, t1);

    AlertEvaluator::Thresholds t2;
    t2.cpu_high_pct = 50.0;
    evaluator.set_thresholds(t2);

    MetricRegistry registry;
    registry.register_metric({"cpu.total_pct", "CPU Total", "%", "pdh", "cpu"});
    registry.push_sample({"cpu.total_pct", 60.0, "%", Quality::Measured, now_unix()});
    evaluator.evaluate(registry);
}
