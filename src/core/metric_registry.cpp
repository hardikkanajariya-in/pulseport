#include "pulseport/metric_registry.h"
#include <spdlog/spdlog.h>

namespace pulseport {

void MetricRegistry::register_metric(const MetricInfo& info) {
    std::lock_guard lock(mutex_);
    if (entries_.contains(info.key)) return;

    Entry entry;
    entry.info = info;
    entry.last_sample = {};
    entry.last_sample.key = info.key;
    entry.last_sample.unit = info.unit;
    entries_.emplace(info.key, std::move(entry));
    spdlog::info("Registered metric: {} ({})", info.key, info.display_name);
}

void MetricRegistry::push_sample(const MetricSample& sample) {
    std::lock_guard lock(mutex_);
    auto it = entries_.find(sample.key);
    if (it == entries_.end()) return;

    it->second.buffer.push(sample);
    it->second.last_sample = sample;
}

std::optional<MetricSample> MetricRegistry::latest(const std::string& key) const {
    std::lock_guard lock(mutex_);
    auto it = entries_.find(key);
    if (it == entries_.end()) return std::nullopt;
    if (it->second.last_sample.ts == 0) return std::nullopt;
    return it->second.last_sample;
}

std::vector<MetricInfo> MetricRegistry::all_metrics() const {
    std::lock_guard lock(mutex_);
    std::vector<MetricInfo> result;
    result.reserve(entries_.size());
    for (const auto& [key, entry] : entries_) {
        result.push_back(entry.info);
    }
    return result;
}

std::vector<MetricSample> MetricRegistry::snapshot() const {
    std::lock_guard lock(mutex_);
    std::vector<MetricSample> result;
    result.reserve(entries_.size());
    for (const auto& [key, entry] : entries_) {
        if (entry.last_sample.ts != 0) {
            result.push_back(entry.last_sample);
        }
    }
    return result;
}

std::vector<MetricSample> MetricRegistry::recent(const std::string& key, size_t count) const {
    std::lock_guard lock(mutex_);
    auto it = entries_.find(key);
    if (it == entries_.end()) return {};

    std::vector<MetricSample> result(count);
    size_t actual = it->second.buffer.peek_last(result.data(), count);
    result.resize(actual);
    return result;
}

} // namespace pulseport
