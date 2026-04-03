#include "pulseport/power_pipeline.h"
#include <cmath>
#include <algorithm>

namespace pulseport {

void PowerPipeline::update(double watts, Quality quality, int64_t ts) {
    current_watts_.store(watts, std::memory_order_relaxed);
    current_quality_.store(static_cast<uint8_t>(quality), std::memory_order_relaxed);

    // Accumulate energy: watts * (delta_seconds / 3600) = Wh
    if (last_ts_ > 0 && ts > last_ts_) {
        double delta_s = static_cast<double>(ts - last_ts_);
        // Clamp delta to prevent absurd values after sleep/resume
        if (delta_s <= 10.0) {
            double wh = watts * (delta_s / 3600.0);
            double current = energy_today_wh_.load(std::memory_order_relaxed);
            energy_today_wh_.store(current + wh, std::memory_order_relaxed);
        }
    }
    last_ts_ = ts;

    // Add to rolling window
    window_[window_head_] = {watts, ts};
    window_head_ = (window_head_ + 1) % kMaxWindow;
    if (window_count_ < kMaxWindow) ++window_count_;
}

double PowerPipeline::avg_watts(int window_seconds) const {
    if (window_count_ == 0) return 0.0;

    int64_t now = last_ts_;
    int64_t cutoff = now - window_seconds;
    double sum = 0.0;
    int count = 0;

    for (int i = 0; i < window_count_; ++i) {
        int idx = (window_head_ - 1 - i + kMaxWindow) % kMaxWindow;
        if (window_[idx].ts < cutoff) break;
        sum += window_[idx].watts;
        ++count;
    }

    return (count > 0) ? (sum / count) : 0.0;
}

void PowerPipeline::reset_daily() {
    energy_today_wh_.store(0.0, std::memory_order_relaxed);
}

double PowerPipeline::accumulated_wh() const {
    return energy_today_wh_.load(std::memory_order_relaxed);
}

} // namespace pulseport
