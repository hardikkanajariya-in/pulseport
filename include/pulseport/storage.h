#pragma once

#include "pulseport/types.h"

#include <mutex>
#include <vector>
#include <functional>

struct sqlite3;

namespace pulseport {

/// Single-writer queue for all persistent inserts and updates.
class StorageWriter {
public:
    explicit StorageWriter(sqlite3* db);

    /// Write current metric snapshot (upsert metric_current).
    void write_current(const std::vector<MetricSample>& samples);

    /// Write 1-minute aggregates.
    void write_1m(const std::vector<MetricAggregate>& aggregates);

    /// Write 15-minute aggregates.
    void write_15m(const std::vector<MetricAggregate>& aggregates);

    /// Write or update daily energy record.
    void write_energy_daily(const std::string& day_local,
                            double energy_wh, double avg_power_w,
                            double peak_power_w, double charge_wh,
                            double discharge_wh, int active_seconds,
                            Quality quality, bool finalized);

    /// Write an event.
    void write_event(int64_t ts, const std::string& severity,
                     const std::string& category, const std::string& title,
                     const std::string& payload_json = "");

    /// Register metric info in metric_registry.
    void write_metric_info(const MetricInfo& info);

private:
    sqlite3* db_;
    std::mutex mutex_;
};

/// Read-only queries for the HTTP API layer.
class StorageReader {
public:
    explicit StorageReader(sqlite3* db);

    /// Query 1m or 15m history for a metric within a time range.
    std::vector<MetricAggregate> query_history(
        const std::string& table,
        const std::string& metric_key,
        int64_t start_ts, int64_t end_ts,
        int limit = 10000) const;

    /// Query daily energy records.
    struct DailyEnergy {
        std::string day_local;
        double energy_wh;
        double avg_power_w;
        double peak_power_w;
        int active_seconds;
        std::string quality;
    };
    std::vector<DailyEnergy> query_energy_daily(
        const std::string& start_day, const std::string& end_day) const;

    /// Query events by time range and optional category.
    struct EventRow {
        int64_t     id;
        int64_t     event_ts;
        std::string severity;
        std::string category;
        std::string title;
        std::string payload_json;
    };
    std::vector<EventRow> query_events(
        int64_t start_ts, int64_t end_ts,
        const std::string& category = "",
        int limit = 1000) const;

    /// Delete history in a time range. Returns rows deleted.
    int delete_history(const std::string& table,
                       int64_t start_ts, int64_t end_ts);

    /// Delete all history, preserving config. Returns total rows deleted.
    int delete_all_history();

private:
    sqlite3* db_;
};

} // namespace pulseport
