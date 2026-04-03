#include <gtest/gtest.h>
#include "pulseport/power_pipeline.h"

using namespace pulseport;

TEST(PowerPipeline, InitialState) {
    PowerPipeline pp;
    EXPECT_DOUBLE_EQ(pp.current_watts(), 0.0);
    EXPECT_EQ(pp.current_quality(), Quality::Unknown);
    EXPECT_DOUBLE_EQ(pp.energy_today_wh(), 0.0);
}

TEST(PowerPipeline, UpdateStoresCurrentPower) {
    PowerPipeline pp;
    pp.update(45.0, Quality::Measured, 1000);
    EXPECT_DOUBLE_EQ(pp.current_watts(), 45.0);
    EXPECT_EQ(pp.current_quality(), Quality::Measured);
}

TEST(PowerPipeline, AvgWattsOverWindow) {
    PowerPipeline pp;

    // Simulate 10 seconds of 50W readings
    for (int i = 0; i < 10; ++i) {
        pp.update(50.0, Quality::Measured, 1000 + i);
    }

    double avg = pp.avg_watts(10);
    EXPECT_NEAR(avg, 50.0, 0.1);
}

TEST(PowerPipeline, AvgWattsPartialWindow) {
    PowerPipeline pp;

    // 5 seconds of 20W, then 5 seconds of 80W
    for (int i = 0; i < 5; ++i) {
        pp.update(20.0, Quality::Measured, 1000 + i);
    }
    for (int i = 5; i < 10; ++i) {
        pp.update(80.0, Quality::Measured, 1000 + i);
    }

    double avg = pp.avg_watts(10);
    EXPECT_NEAR(avg, 50.0, 1.0);
}

TEST(PowerPipeline, ResetDailyClearsEnergy) {
    PowerPipeline pp;

    pp.update(100.0, Quality::Measured, 1000);
    pp.update(100.0, Quality::Measured, 1001);
    pp.reset_daily();

    EXPECT_DOUBLE_EQ(pp.energy_today_wh(), 0.0);
    // Current power should still be readable
    EXPECT_DOUBLE_EQ(pp.current_watts(), 100.0);
}

TEST(PowerPipeline, AvgWattsEmptyWindow) {
    PowerPipeline pp;
    // No data means average should be 0
    EXPECT_DOUBLE_EQ(pp.avg_watts(60), 0.0);
}

TEST(PowerPipeline, QualityTracking) {
    PowerPipeline pp;
    pp.update(30.0, Quality::Estimated, 1000);
    EXPECT_EQ(pp.current_quality(), Quality::Estimated);

    pp.update(40.0, Quality::Measured, 1001);
    EXPECT_EQ(pp.current_quality(), Quality::Measured);
}
