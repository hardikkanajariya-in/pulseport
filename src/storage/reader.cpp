#include "pulseport/storage.h"

#include <sqlite3.h>
#include <spdlog/spdlog.h>

namespace pulseport {

StorageReader::StorageReader(sqlite3* db) : db_(db) {}

std::vector<MetricAggregate> StorageReader::query_history(
    const std::string& table,
    const std::string& metric_key,
    int64_t start_ts, int64_t end_ts,
    int limit) const {

    // Whitelist table name to prevent SQL injection
    if (table != "metric_1m" && table != "metric_15m") {
        spdlog::error("Invalid table name for history query: {}", table);
        return {};
    }

    std::string sql = "SELECT bucket_ts, metric_key, min_value, max_value, "
                      "avg_value, sample_count, quality FROM " + table +
                      " WHERE metric_key = ? AND bucket_ts >= ? AND bucket_ts <= ?"
                      " ORDER BY bucket_ts ASC LIMIT ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare history query: {}", sqlite3_errmsg(db_));
        return {};
    }

    sqlite3_bind_text(stmt, 1, metric_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, start_ts);
    sqlite3_bind_int64(stmt, 3, end_ts);
    sqlite3_bind_int(stmt, 4, limit);

    std::vector<MetricAggregate> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MetricAggregate agg;
        agg.bucket_ts    = sqlite3_column_int64(stmt, 0);
        agg.key          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        agg.min_value    = sqlite3_column_double(stmt, 2);
        agg.max_value    = sqlite3_column_double(stmt, 3);
        agg.avg_value    = sqlite3_column_double(stmt, 4);
        agg.sample_count = sqlite3_column_int(stmt, 5);
        auto q = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        agg.quality = quality_from_string(q ? q : "unknown");
        results.push_back(std::move(agg));
    }

    sqlite3_finalize(stmt);
    return results;
}

std::vector<StorageReader::DailyEnergy> StorageReader::query_energy_daily(
    const std::string& start_day, const std::string& end_day) const {

    const char* sql = R"(
        SELECT day_local, energy_wh, avg_power_w, peak_power_w, active_seconds, quality
        FROM energy_daily
        WHERE day_local >= ? AND day_local <= ?
        ORDER BY day_local ASC
    )";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, start_day.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, end_day.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<DailyEnergy> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DailyEnergy d;
        d.day_local      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        d.energy_wh      = sqlite3_column_double(stmt, 1);
        d.avg_power_w    = sqlite3_column_double(stmt, 2);
        d.peak_power_w   = sqlite3_column_double(stmt, 3);
        d.active_seconds = sqlite3_column_int(stmt, 4);
        auto q = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        d.quality = q ? q : "unknown";
        results.push_back(std::move(d));
    }

    sqlite3_finalize(stmt);
    return results;
}

std::vector<StorageReader::EventRow> StorageReader::query_events(
    int64_t start_ts, int64_t end_ts,
    const std::string& category, int limit) const {

    std::string sql = "SELECT id, event_ts, severity, category, title, payload_json "
                      "FROM events WHERE event_ts >= ? AND event_ts <= ?";

    if (!category.empty()) {
        sql += " AND category = ?";
    }
    sql += " ORDER BY event_ts DESC LIMIT ?";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    int param = 1;
    sqlite3_bind_int64(stmt, param++, start_ts);
    sqlite3_bind_int64(stmt, param++, end_ts);
    if (!category.empty()) {
        sqlite3_bind_text(stmt, param++, category.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, param, limit);

    std::vector<EventRow> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EventRow e;
        e.id           = sqlite3_column_int64(stmt, 0);
        e.event_ts     = sqlite3_column_int64(stmt, 1);
        e.severity     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        e.category     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        e.title        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        auto p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        e.payload_json = p ? p : "";
        results.push_back(std::move(e));
    }

    sqlite3_finalize(stmt);
    return results;
}

int StorageReader::delete_history(const std::string& table,
                                   int64_t start_ts, int64_t end_ts) {
    if (table != "metric_1m" && table != "metric_15m" && table != "events") {
        spdlog::error("Invalid table for deletion: {}", table);
        return -1;
    }

    std::string ts_col = (table == "events") ? "event_ts" : "bucket_ts";
    std::string sql = "DELETE FROM " + table + " WHERE " + ts_col +
                      " >= ? AND " + ts_col + " <= ?";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, start_ts);
    sqlite3_bind_int64(stmt, 2, end_ts);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    int deleted = sqlite3_changes(db_);

    const char* audit_sql = R"(
        INSERT INTO deletions_audit (scope, range_start, range_end, note)
        VALUES (?, ?, ?, ?)
    )";
    sqlite3_stmt* audit = nullptr;
    sqlite3_prepare_v2(db_, audit_sql, -1, &audit, nullptr);
    sqlite3_bind_text(audit, 1, table.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(audit, 2, start_ts);
    sqlite3_bind_int64(audit, 3, end_ts);
    std::string note = "Deleted " + std::to_string(deleted) + " rows";
    sqlite3_bind_text(audit, 4, note.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(audit);
    sqlite3_finalize(audit);

    spdlog::info("Deleted {} rows from {} [{}, {}]", deleted, table, start_ts, end_ts);
    return deleted;
}

int StorageReader::delete_all_history() {
    int total = 0;
    const char* tables[] = {"metric_1m", "metric_15m", "metric_current",
                            "energy_daily", "events", nullptr};

    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    for (int i = 0; tables[i]; ++i) {
        std::string sql = std::string("DELETE FROM ") + tables[i];
        sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
        total += sqlite3_changes(db_);
    }

    const char* audit_sql = R"(
        INSERT INTO deletions_audit (scope, note)
        VALUES ('all_history', ?)
    )";
    sqlite3_stmt* audit = nullptr;
    sqlite3_prepare_v2(db_, audit_sql, -1, &audit, nullptr);
    std::string note = "Deleted all history: " + std::to_string(total) + " total rows";
    sqlite3_bind_text(audit, 1, note.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(audit);
    sqlite3_finalize(audit);

    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);

    spdlog::info("Deleted all history: {} total rows", total);
    return total;
}

int StorageReader::cleanup_expired(const RetentionConfig& rc) {
    int64_t now = now_unix();
    int total_deleted = 0;

    auto purge_table = [&](const char* table, const char* ts_col, int days) {
        if (days <= 0) return;
        int64_t cutoff = now - static_cast<int64_t>(days) * 86400;
        std::string sql = std::string("DELETE FROM ") + table +
                          " WHERE " + ts_col + " < ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, cutoff);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            int deleted = sqlite3_changes(db_);
            if (deleted > 0) {
                spdlog::info("Retention cleanup: {} rows from {}", deleted, table);
                total_deleted += deleted;
            }
        }
    };

    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    purge_table("metric_1m",    "bucket_ts", rc.retention_1m_days);
    purge_table("metric_15m",   "bucket_ts", rc.retention_15m_days);
    purge_table("events",       "event_ts",  rc.retention_events_days);

    // energy_daily uses ISO date string, compare differently
    if (rc.retention_daily_days > 0) {
        int64_t cutoff_ts = now - static_cast<int64_t>(rc.retention_daily_days) * 86400;
        time_t t = static_cast<time_t>(cutoff_ts);
        struct tm tm_buf{};
        localtime_s(&tm_buf, &t);
        char date_str[11];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm_buf);

        const char* sql = "DELETE FROM energy_daily WHERE day_local < ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, date_str, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            int deleted = sqlite3_changes(db_);
            if (deleted > 0) {
                spdlog::info("Retention cleanup: {} rows from energy_daily", deleted);
                total_deleted += deleted;
            }
        }
    }

    if (total_deleted > 0) {
        const char* audit_sql = R"(
            INSERT INTO deletions_audit (scope, note)
            VALUES ('retention_cleanup', ?)
        )";
        sqlite3_stmt* audit = nullptr;
        if (sqlite3_prepare_v2(db_, audit_sql, -1, &audit, nullptr) == SQLITE_OK) {
            std::string note = "Automated retention cleanup: " +
                               std::to_string(total_deleted) + " total rows";
            sqlite3_bind_text(audit, 1, note.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(audit);
            sqlite3_finalize(audit);
        }
    }

    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);

    if (total_deleted > 0) {
        spdlog::info("Retention cleanup complete: {} total rows deleted", total_deleted);
    }
    return total_deleted;
}

} // namespace pulseport
