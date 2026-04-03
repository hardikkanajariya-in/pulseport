#include "pulseport/storage.h"

#include <sqlite3.h>
#include <spdlog/spdlog.h>

namespace pulseport {

StorageWriter::StorageWriter(sqlite3* db) : db_(db) {}

void StorageWriter::write_current(const std::vector<MetricSample>& samples) {
    std::lock_guard lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO metric_current (metric_key, ts, value_real, unit, quality)
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    for (const auto& s : samples) {
        sqlite3_bind_text(stmt, 1, s.key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, s.ts);
        sqlite3_bind_double(stmt, 3, s.value);
        sqlite3_bind_text(stmt, 4, s.unit.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, quality_to_string(s.quality), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
}

void StorageWriter::write_1m(const std::vector<MetricAggregate>& aggregates) {
    std::lock_guard lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO metric_1m
        (bucket_ts, metric_key, min_value, max_value, avg_value, sample_count, quality)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    for (const auto& a : aggregates) {
        sqlite3_bind_int64(stmt, 1, a.bucket_ts);
        sqlite3_bind_text(stmt, 2, a.key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, a.min_value);
        sqlite3_bind_double(stmt, 4, a.max_value);
        sqlite3_bind_double(stmt, 5, a.avg_value);
        sqlite3_bind_int(stmt, 6, a.sample_count);
        sqlite3_bind_text(stmt, 7, quality_to_string(a.quality), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
}

void StorageWriter::write_15m(const std::vector<MetricAggregate>& aggregates) {
    std::lock_guard lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO metric_15m
        (bucket_ts, metric_key, min_value, max_value, avg_value, sample_count, quality)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    for (const auto& a : aggregates) {
        sqlite3_bind_int64(stmt, 1, a.bucket_ts);
        sqlite3_bind_text(stmt, 2, a.key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, a.min_value);
        sqlite3_bind_double(stmt, 4, a.max_value);
        sqlite3_bind_double(stmt, 5, a.avg_value);
        sqlite3_bind_int(stmt, 6, a.sample_count);
        sqlite3_bind_text(stmt, 7, quality_to_string(a.quality), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
}

void StorageWriter::write_energy_daily(const std::string& day_local,
                                        double energy_wh, double avg_power_w,
                                        double peak_power_w, double charge_wh,
                                        double discharge_wh, int active_seconds,
                                        Quality quality, bool finalized) {
    std::lock_guard lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO energy_daily
        (day_local, energy_wh, avg_power_w, peak_power_w, charge_energy_wh,
         discharge_energy_wh, active_seconds, quality, finalized, updated_at_utc)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
    )";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, day_local.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, energy_wh);
    sqlite3_bind_double(stmt, 3, avg_power_w);
    sqlite3_bind_double(stmt, 4, peak_power_w);
    sqlite3_bind_double(stmt, 5, charge_wh);
    sqlite3_bind_double(stmt, 6, discharge_wh);
    sqlite3_bind_int(stmt, 7, active_seconds);
    sqlite3_bind_text(stmt, 8, quality_to_string(quality), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, finalized ? 1 : 0);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void StorageWriter::write_event(int64_t ts, const std::string& severity,
                                 const std::string& category, const std::string& title,
                                 const std::string& payload_json) {
    std::lock_guard lock(mutex_);

    const char* sql = R"(
        INSERT INTO events (event_ts, severity, category, title, payload_json)
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_int64(stmt, 1, ts);
    sqlite3_bind_text(stmt, 2, severity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, title.c_str(), -1, SQLITE_TRANSIENT);
    if (!payload_json.empty()) {
        sqlite3_bind_text(stmt, 5, payload_json.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 5);
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void StorageWriter::write_metric_info(const MetricInfo& info) {
    std::lock_guard lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO metric_registry
        (metric_key, display_name, unit, source, category)
        VALUES (?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, info.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, info.display_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, info.unit.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, info.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, info.category.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

} // namespace pulseport
