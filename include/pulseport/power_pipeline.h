#pragma once

#include "pulseport/types.h"
#include <atomic>

namespace pulseport {

/// Tracks instantaneous power and accumulates energy over time.
class PowerPipeline {
public:
    /// Update with a new power reading. Call at each sample interval.
    void update(double watts, Quality quality, int64_t ts);

    /// Get the current power draw in watts.
    double current_watts() const { return current_watts_.load(std::memory_order_relaxed); }

    /// Get the current quality level.
    Quality current_quality() const {
        return static_cast<Quality>(current_quality_.load(std::memory_order_relaxed));
    }

    /// Get rolling average power over the last N seconds.
    double avg_watts(int window_seconds) const;

    /// Get accumulated energy today in Wh.
    double energy_today_wh() const { return energy_today_wh_.load(std::memory_order_relaxed); }

    /// Reset daily accumulator (called at day boundary).
    void reset_daily();

    /// Get accumulated energy since last reset in Wh.
    double accumulated_wh() const;

private:
    std::atomic<double>  current_watts_{0.0};
    std::atomic<uint8_t> current_quality_{static_cast<uint8_t>(Quality::Unknown)};
    std::atomic<double>  energy_today_wh_{0.0};

    // Rolling window for averages (lock-free not needed, updated from single thread)
    struct PowerSample {
        double watts;
        int64_t ts;
    };
    static constexpr int kMaxWindow = 900; // 15 minutes of 1s samples
    PowerSample window_[kMaxWindow]{};
    int window_head_ = 0;
    int window_count_ = 0;
    int64_t last_ts_ = 0;
};

} // namespace pulseport
