#pragma once

#include "pulseport/metric_registry.h"
#include "pulseport/storage.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace pulseport {

/// Evaluates metric samples against configurable thresholds and emits
/// alert events when sustained violations are detected.
class AlertEvaluator {
public:
    struct Thresholds {
        double cpu_high_pct        = 90.0;
        int    cpu_sustained_min   = 5;
        double mem_high_pct        = 90.0;
        int    mem_sustained_min   = 5;
        double battery_low_pct    = 15.0;
        double power_high_w       = 100.0;
        int    cooldown_minutes   = 30;
    };

    AlertEvaluator(StorageWriter& writer, const Thresholds& thresholds = {});

    /// Evaluate current metrics against thresholds. Call every 60s.
    void evaluate(const MetricRegistry& registry);

    void set_thresholds(const Thresholds& t);

private:
    struct AlertState {
        int violation_count = 0; // consecutive minutes over threshold
        int64_t last_alert_ts = 0;
    };

    void check_threshold(const std::string& metric_key, double value,
                          double threshold, bool above,
                          int sustained_minutes,
                          const std::string& alert_category,
                          const std::string& alert_title_fmt);

    StorageWriter& writer_;
    Thresholds thresholds_;
    std::unordered_map<std::string, AlertState> state_;
};

} // namespace pulseport
