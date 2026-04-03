#include "pulseport/aggregator.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace pulseport {

Aggregator::Aggregator(MetricRegistry& registry, StorageWriter& writer)
    : registry_(registry), writer_(writer), current_bucket_ts_(0) {}

int64_t Aggregator::bucket_1m(int64_t ts) {
    return ts - (ts % 60);
}

int64_t Aggregator::bucket_15m(int64_t ts) {
    return ts - (ts % 900);
}

void Aggregator::WorkingBucket::add(double value, Quality q) {
    if (count == 0) {
        min_val = max_val = sum = value;
    } else {
        min_val = std::min(min_val, value);
        max_val = std::max(max_val, value);
        sum += value;
    }
    ++count;
    if (static_cast<uint8_t>(q) > static_cast<uint8_t>(worst_quality)) {
        worst_quality = q;
    }
}

MetricAggregate Aggregator::WorkingBucket::to_aggregate(
    const std::string& key, int64_t bucket_ts) const {
    MetricAggregate agg;
    agg.key = key;
    agg.bucket_ts = bucket_ts;
    agg.min_value = min_val;
    agg.max_value = max_val;
    agg.avg_value = (count > 0) ? (sum / count) : 0.0;
    agg.sample_count = count;
    agg.quality = worst_quality;
    return agg;
}

void Aggregator::WorkingBucket::reset() {
    min_val = max_val = sum = 0.0;
    count = 0;
    worst_quality = Quality::Measured;
}

void Aggregator::accumulate(const MetricSample& sample) {
    std::lock_guard lock(mutex_);

    int64_t bts = bucket_1m(sample.ts);

    // If bucket changed, flush old buckets
    if (current_bucket_ts_ != 0 && bts != current_bucket_ts_) {
        flush_1m();
    }
    current_bucket_ts_ = bts;

    buckets_[sample.key].add(sample.value, sample.quality);
}

void Aggregator::flush_1m() {
    // Called with mutex_ held from accumulate(), or externally
    std::vector<MetricAggregate> aggregates;
    aggregates.reserve(buckets_.size());

    for (const auto& [key, bucket] : buckets_) {
        if (bucket.count > 0) {
            aggregates.push_back(bucket.to_aggregate(key, current_bucket_ts_));
        }
    }

    if (!aggregates.empty()) {
        writer_.write_1m(aggregates);
        spdlog::debug("Flushed {} 1m aggregates for bucket {}",
                       aggregates.size(), current_bucket_ts_);
    }

    // Reset all buckets
    for (auto& [key, bucket] : buckets_) {
        bucket.reset();
    }
}

void Aggregator::flush_15m(StorageReader& reader) {
    int64_t now = now_unix();
    int64_t bucket_end = bucket_15m(now);
    int64_t bucket_start = bucket_end - 900;

    auto metrics = registry_.all_metrics();
    std::vector<MetricAggregate> aggregates;

    for (const auto& info : metrics) {
        auto data = reader.query_history("metric_1m", info.key, bucket_start, bucket_end);
        if (data.empty()) continue;

        MetricAggregate agg;
        agg.key = info.key;
        agg.bucket_ts = bucket_start;
        agg.min_value = data[0].min_value;
        agg.max_value = data[0].max_value;
        double sum = 0;
        int total_samples = 0;
        Quality worst = Quality::Measured;

        for (const auto& d : data) {
            agg.min_value = std::min(agg.min_value, d.min_value);
            agg.max_value = std::max(agg.max_value, d.max_value);
            sum += d.avg_value * d.sample_count;
            total_samples += d.sample_count;
            if (static_cast<uint8_t>(d.quality) > static_cast<uint8_t>(worst)) {
                worst = d.quality;
            }
        }

        agg.avg_value = (total_samples > 0) ? (sum / total_samples) : 0.0;
        agg.sample_count = total_samples;
        agg.quality = worst;
        aggregates.push_back(std::move(agg));
    }

    if (!aggregates.empty()) {
        writer_.write_15m(aggregates);
        spdlog::debug("Flushed {} 15m aggregates", aggregates.size());
    }
}

void Aggregator::finalize_daily(const std::string& day_local, StorageReader& reader) {
    // Parse day_local "YYYY-MM-DD" to get Unix timestamp range
    struct tm tm_buf{};
    int y, mo, d;
    if (sscanf(day_local.c_str(), "%d-%d-%d", &y, &mo, &d) != 3) {
        spdlog::warn("finalize_daily: invalid day format '{}'", day_local);
        return;
    }
    tm_buf.tm_year = y - 1900;
    tm_buf.tm_mon = mo - 1;
    tm_buf.tm_mday = d;
    tm_buf.tm_isdst = -1;
    time_t day_start = mktime(&tm_buf);
    if (day_start == -1) return;

    int64_t start_ts = static_cast<int64_t>(day_start);
    int64_t end_ts = start_ts + 86400;

    // Query 1m power data for the day
    auto power_data = reader.query_history("metric_1m", "power.current_w",
                                            start_ts, end_ts);

    double total_wh = 0.0;
    double peak_w = 0.0;
    double sum_w = 0.0;
    int active_seconds = 0;
    Quality worst = Quality::Measured;

    for (const auto& agg : power_data) {
        // Each 1m aggregate: avg_value is avg watts over that minute
        double watts = agg.avg_value;
        total_wh += watts * (60.0 / 3600.0); // 1 minute of energy in Wh
        peak_w = std::max(peak_w, agg.max_value);
        sum_w += watts * agg.sample_count;
        active_seconds += 60;
        if (static_cast<uint8_t>(agg.quality) > static_cast<uint8_t>(worst)) {
            worst = agg.quality;
        }
    }

    double avg_w = power_data.empty() ? 0.0 : (sum_w / std::max(1, active_seconds));

    writer_.write_energy_daily(day_local, total_wh, avg_w, peak_w,
                                0.0, 0.0, active_seconds, worst, true);

    spdlog::info("Daily energy finalized for {}: {:.1f} Wh, avg {:.1f} W, peak {:.1f} W",
                  day_local, total_wh, avg_w, peak_w);
}

} // namespace pulseport
