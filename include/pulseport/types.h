#pragma once

#include <string>
#include <cstdint>
#include <chrono>

namespace pulseport {

using SteadyClock = std::chrono::steady_clock;
using SystemClock = std::chrono::system_clock;
using TimePoint   = SystemClock::time_point;

/// Convert system_clock time_point to Unix epoch seconds.
inline int64_t to_unix(TimePoint tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        tp.time_since_epoch()
    ).count();
}

/// Convert Unix epoch seconds to system_clock time_point.
inline TimePoint from_unix(int64_t epoch) {
    return TimePoint{std::chrono::seconds{epoch}};
}

/// Returns current UTC Unix epoch seconds.
inline int64_t now_unix() {
    return to_unix(SystemClock::now());
}

/// Data quality for a metric sample.
enum class Quality : uint8_t {
    Measured,   // Direct hardware or OS reading
    Derived,    // Computed from other measurements (e.g. watts from battery delta)
    Estimated,  // Heuristic approximation
    Unknown     // Sensor unavailable or data missing
};

inline const char* quality_to_string(Quality q) {
    switch (q) {
        case Quality::Measured:  return "measured";
        case Quality::Derived:   return "derived";
        case Quality::Estimated: return "estimated";
        case Quality::Unknown:   return "unknown";
    }
    return "unknown";
}

inline Quality quality_from_string(const std::string& s) {
    if (s == "measured")  return Quality::Measured;
    if (s == "derived")   return Quality::Derived;
    if (s == "estimated") return Quality::Estimated;
    return Quality::Unknown;
}

/// A single metric sample.
struct MetricSample {
    std::string key;
    double      value = 0.0;
    std::string unit;
    Quality     quality = Quality::Unknown;
    int64_t     ts = 0;  // Unix epoch seconds
};

/// Aggregated bucket for 1m / 15m rollups.
struct MetricAggregate {
    std::string key;
    int64_t     bucket_ts = 0;
    double      min_value = 0.0;
    double      max_value = 0.0;
    double      avg_value = 0.0;
    int         sample_count = 0;
    Quality     quality = Quality::Unknown;
};

/// Metric registry entry (static metadata).
struct MetricInfo {
    std::string key;
    std::string display_name;
    std::string unit;
    std::string source;
    std::string category;
};

} // namespace pulseport
