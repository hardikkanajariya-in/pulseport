#pragma once

#include "pulseport/types.h"
#include "pulseport/ring_buffer.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pulseport {

/// Per-metric live buffer: 1800 samples = 30 minutes at 1s intervals.
using LiveBuffer = RingBuffer<MetricSample, 2048>;

/// Central registry of all known metrics and their live ring buffers.
class MetricRegistry {
public:
    /// Register a new metric. Safe to call multiple times with same key.
    void register_metric(const MetricInfo& info);

    /// Push a live sample into the metric's ring buffer.
    void push_sample(const MetricSample& sample);

    /// Get the latest sample for a metric. Returns nullopt if unknown.
    std::optional<MetricSample> latest(const std::string& key) const;

    /// Get all registered metric infos.
    std::vector<MetricInfo> all_metrics() const;

    /// Get a snapshot of the latest value for every registered metric.
    std::vector<MetricSample> snapshot() const;

    /// Read last N samples from a metric's live buffer.
    std::vector<MetricSample> recent(const std::string& key, size_t count) const;

private:
    struct Entry {
        MetricInfo info;
        LiveBuffer buffer;
        MetricSample last_sample;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace pulseport
