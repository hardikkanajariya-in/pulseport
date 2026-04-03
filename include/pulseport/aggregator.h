#pragma once

#include "pulseport/metric_registry.h"
#include "pulseport/storage.h"
#include "pulseport/types.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace pulseport {

/// Computes 1m, 15m, and daily rollup aggregates from live samples.
/// Runs on its own thread driven by the sampler's 60s cadence.
class Aggregator {
public:
    Aggregator(MetricRegistry& registry, StorageWriter& writer);

    /// Called every second: accumulate sample into working bucket.
    void accumulate(const MetricSample& sample);

    /// Called every 60 seconds: flush 1-minute buckets to storage.
    void flush_1m();

    /// Called every 15 minutes: build 15-minute rollups from stored 1m data.
    void flush_15m(StorageReader& reader);

    /// Called at day boundary or shutdown: finalize daily energy.
    void finalize_daily(const std::string& day_local, StorageReader& reader);

    /// Get the current working bucket timestamp (floor to minute).
    static int64_t bucket_1m(int64_t ts);

    /// Get the current 15-minute bucket timestamp.
    static int64_t bucket_15m(int64_t ts);

private:
    struct WorkingBucket {
        double min_val = 0.0;
        double max_val = 0.0;
        double sum = 0.0;
        int    count = 0;
        Quality worst_quality = Quality::Measured;

        void add(double value, Quality q);
        MetricAggregate to_aggregate(const std::string& key, int64_t bucket_ts) const;
        void reset();
    };

    MetricRegistry& registry_;
    StorageWriter& writer_;
    std::mutex mutex_;
    std::unordered_map<std::string, WorkingBucket> buckets_;
    int64_t current_bucket_ts_ = 0;
};

} // namespace pulseport
