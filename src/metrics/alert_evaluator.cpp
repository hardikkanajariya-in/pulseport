#include "pulseport/alert_evaluator.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace pulseport {

AlertEvaluator::AlertEvaluator(StorageWriter& writer, const Thresholds& thresholds)
    : writer_(writer), thresholds_(thresholds) {}

void AlertEvaluator::set_thresholds(const Thresholds& t) {
    thresholds_ = t;
}

void AlertEvaluator::check_threshold(
    const std::string& metric_key, double value,
    double threshold, bool above,
    int sustained_minutes,
    const std::string& alert_category,
    const std::string& alert_title_fmt) {

    auto& st = state_[metric_key];
    bool violated = above ? (value >= threshold) : (value <= threshold);

    if (violated) {
        ++st.violation_count;
    } else {
        st.violation_count = 0;
        return;
    }

    if (st.violation_count < sustained_minutes) return;

    int64_t now = now_unix();
    int64_t cooldown_s = static_cast<int64_t>(thresholds_.cooldown_minutes) * 60;
    if (st.last_alert_ts > 0 && (now - st.last_alert_ts) < cooldown_s) {
        return;
    }

    std::string title = fmt::format(fmt::runtime(alert_title_fmt), value);
    writer_.write_event(now, "warn", alert_category, title);
    st.last_alert_ts = now;
    st.violation_count = 0;

    spdlog::warn("Alert fired: {} (value={:.1f})", alert_category, value);
}

void AlertEvaluator::evaluate(const MetricRegistry& registry) {
    auto snap = registry.snapshot();

    for (const auto& sample : snap) {
        if (sample.key == "cpu.total_pct") {
            check_threshold(sample.key, sample.value,
                            thresholds_.cpu_high_pct, true,
                            thresholds_.cpu_sustained_min,
                            "cpu_high",
                            "CPU usage sustained at {:.0f}%");
        }
        else if (sample.key == "mem.used_pct") {
            check_threshold(sample.key, sample.value,
                            thresholds_.mem_high_pct, true,
                            thresholds_.mem_sustained_min,
                            "mem_high",
                            "Memory usage sustained at {:.0f}%");
        }
        else if (sample.key == "battery.level_pct") {
            check_threshold(sample.key, sample.value,
                            thresholds_.battery_low_pct, false,
                            1, // alert immediately
                            "battery_low",
                            "Battery level critically low at {:.0f}%");
        }
        else if (sample.key == "power.current_w" && sample.quality != Quality::Unknown) {
            check_threshold(sample.key, sample.value,
                            thresholds_.power_high_w, true,
                            thresholds_.cpu_sustained_min,
                            "power_high",
                            "Power draw elevated at {:.1f} W");
        }
    }
}

} // namespace pulseport
