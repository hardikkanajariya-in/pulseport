#pragma once

#include <string>
#include <chrono>
#include <atomic>

namespace pulseport {

/// Internal self-metrics for the diagnostics page.
struct SelfMetrics {
    std::string  version;
    std::string  build_hash;

    int64_t      start_time_unix = 0;
    int64_t      last_sample_time = 0;
    int64_t      last_flush_time = 0;

    int          ws_connections = 0;
    int64_t      db_size_bytes = 0;
    int64_t      wal_size_bytes = 0;

    int          registered_metrics = 0;
    int          active_collectors = 0;

    std::string  service_mode;   // "service" or "console"

    /// Uptime in seconds.
    int64_t uptime_seconds() const;
};

/// Global self-metrics singleton.
SelfMetrics& self_metrics();

} // namespace pulseport
