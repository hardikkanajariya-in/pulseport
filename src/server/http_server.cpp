#include "pulseport/http_server.h"
#include "pulseport/self_metrics.h"
#include "pulseport/version.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <mutex>
#include <set>
#include <thread>

using json = nlohmann::json;

namespace pulseport {

// ── JSON helpers ────────────────────────────────────────────────

static json sample_to_json(const MetricSample& s) {
    return {
        {"key",     s.key},
        {"value",   s.value},
        {"unit",    s.unit},
        {"quality", quality_to_string(s.quality)},
        {"ts",      s.ts}
    };
}

static json aggregate_to_json(const MetricAggregate& a) {
    return {
        {"key",          a.key},
        {"bucket_ts",    a.bucket_ts},
        {"min",          a.min_value},
        {"max",          a.max_value},
        {"avg",          a.avg_value},
        {"sampleCount",  a.sample_count},
        {"quality",      quality_to_string(a.quality)}
    };
}

// ── Implementation ──────────────────────────────────────────────

struct HttpServer::Impl {
    httplib::Server svr;
    MetricRegistry& registry;
    StorageReader&  reader;
    StorageWriter&  writer;
    Database&       database;
    std::string     web_dir;

    // Track active WebSocket sessions (placeholder for future WS support)
    std::atomic<int> ws_connections{0};

    Impl(MetricRegistry& r, StorageReader& rd, StorageWriter& wr, Database& db)
        : registry(r), reader(rd), writer(wr), database(db) {}

    void setup_routes() {
        setup_api_routes();
        setup_static_serving();
    }

    void setup_api_routes() {
        // Health
        svr.Get("/api/v1/health", [this](const httplib::Request&, httplib::Response& res) {
            auto& sm = self_metrics();
            json body = {
                {"status",  "ok"},
                {"version", PULSEPORT_VERSION},
                {"uptime",  sm.uptime_seconds()}
            };
            res.set_content(body.dump(), "application/json");
        });

        // System info
        svr.Get("/api/v1/system/info", [this](const httplib::Request&, httplib::Response& res) {
            auto metrics = registry.all_metrics();
            json caps = json::array();
            for (const auto& m : metrics) {
                caps.push_back({
                    {"key", m.key}, {"name", m.display_name},
                    {"unit", m.unit}, {"category", m.category}
                });
            }
            json body = {{"metrics", caps}};
            res.set_content(body.dump(), "application/json");
        });

        // Live snapshot
        svr.Get("/api/v1/live/snapshot", [this](const httplib::Request&, httplib::Response& res) {
            auto snap = registry.snapshot();
            json metrics = json::array();
            for (const auto& s : snap) {
                metrics.push_back(sample_to_json(s));
            }
            json body = {
                {"type", "snapshot"},
                {"tsUtc", now_unix()},
                {"metrics", metrics}
            };
            res.set_content(body.dump(), "application/json");
        });

        // History query
        svr.Get("/api/v1/history", [this](const httplib::Request& req, httplib::Response& res) {
            auto metric_key = req.get_param_value("metric");
            auto table      = req.get_param_value("resolution");
            auto start_str  = req.get_param_value("start");
            auto end_str    = req.get_param_value("end");

            if (metric_key.empty() || start_str.empty() || end_str.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Missing required params: metric, start, end"})",
                                "application/json");
                return;
            }

            if (table.empty()) table = "metric_1m";
            if (table != "metric_1m" && table != "metric_15m") {
                res.status = 400;
                res.set_content(R"({"error":"resolution must be metric_1m or metric_15m"})",
                                "application/json");
                return;
            }

            int64_t start_ts = 0, end_ts = 0;
            try {
                start_ts = std::stoll(start_str);
                end_ts   = std::stoll(end_str);
            } catch (...) {
                res.status = 400;
                res.set_content(R"({"error":"start and end must be Unix timestamps"})",
                                "application/json");
                return;
            }

            auto data = reader.query_history(table, metric_key, start_ts, end_ts);
            json rows = json::array();
            for (const auto& a : data) {
                rows.push_back(aggregate_to_json(a));
            }
            json body = {{"data", rows}, {"count", rows.size()}};
            res.set_content(body.dump(), "application/json");
        });

        // Daily energy
        svr.Get("/api/v1/energy/daily", [this](const httplib::Request& req, httplib::Response& res) {
            auto start_day = req.get_param_value("start");
            auto end_day   = req.get_param_value("end");

            if (start_day.empty() || end_day.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"Missing start and end date (YYYY-MM-DD)"})",
                                "application/json");
                return;
            }

            auto data = reader.query_energy_daily(start_day, end_day);
            json rows = json::array();
            for (const auto& d : data) {
                rows.push_back({
                    {"day",           d.day_local},
                    {"energyWh",      d.energy_wh},
                    {"avgPowerW",     d.avg_power_w},
                    {"peakPowerW",    d.peak_power_w},
                    {"activeSeconds", d.active_seconds},
                    {"quality",       d.quality}
                });
            }
            json body = {{"data", rows}, {"count", rows.size()}};
            res.set_content(body.dump(), "application/json");
        });

        // Events
        svr.Get("/api/v1/events", [this](const httplib::Request& req, httplib::Response& res) {
            auto start_str = req.get_param_value("start");
            auto end_str   = req.get_param_value("end");
            auto category  = req.get_param_value("category");

            int64_t start_ts = 0, end_ts = now_unix();
            if (!start_str.empty()) {
                try { start_ts = std::stoll(start_str); } catch (...) {}
            }
            if (!end_str.empty()) {
                try { end_ts = std::stoll(end_str); } catch (...) {}
            }

            auto data = reader.query_events(start_ts, end_ts, category);
            json rows = json::array();
            for (const auto& e : data) {
                json row = {
                    {"id",       e.id},
                    {"ts",       e.event_ts},
                    {"severity", e.severity},
                    {"category", e.category},
                    {"title",    e.title}
                };
                if (!e.payload_json.empty()) {
                    try { row["payload"] = json::parse(e.payload_json); }
                    catch (...) { row["payload"] = e.payload_json; }
                }
                rows.push_back(std::move(row));
            }
            json body = {{"data", rows}, {"count", rows.size()}};
            res.set_content(body.dump(), "application/json");
        });

        // Delete history
        svr.Post("/api/v1/history/delete", [this](const httplib::Request& req, httplib::Response& res) {
            // Origin check for CSRF protection
            auto origin = req.get_header_value("Origin");
            if (!origin.empty() && origin.find("127.0.0.1") == std::string::npos
                                && origin.find("localhost") == std::string::npos) {
                res.status = 403;
                res.set_content(R"({"error":"Invalid origin"})", "application/json");
                return;
            }

            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                res.status = 400;
                res.set_content(R"({"error":"Invalid JSON body"})", "application/json");
                return;
            }

            std::string scope = body.value("scope", "");
            if (scope == "all") {
                int deleted = reader.delete_all_history();
                res.set_content(json({{"deleted", deleted}}).dump(), "application/json");
            } else {
                std::string table = body.value("table", "metric_1m");
                int64_t start_ts  = body.value("start", int64_t(0));
                int64_t end_ts    = body.value("end", int64_t(0));
                if (start_ts == 0 || end_ts == 0 || start_ts > end_ts) {
                    res.status = 400;
                    res.set_content(R"({"error":"Invalid time range"})", "application/json");
                    return;
                }
                int deleted = reader.delete_history(table, start_ts, end_ts);
                if (deleted < 0) {
                    res.status = 400;
                    res.set_content(R"({"error":"Invalid table"})", "application/json");
                    return;
                }
                res.set_content(json({{"deleted", deleted}}).dump(), "application/json");
            }
        });

        // Diagnostics
        svr.Get("/api/v1/diagnostics", [this](const httplib::Request&, httplib::Response& res) {
            auto& sm = self_metrics();
            sm.db_size_bytes  = database.file_size_bytes();
            sm.wal_size_bytes = database.wal_size_bytes();
            sm.ws_connections = ws_connections.load();
            sm.registered_metrics = static_cast<int>(registry.all_metrics().size());

            json body = {
                {"version",           sm.version},
                {"uptime",            sm.uptime_seconds()},
                {"lastSampleTime",    sm.last_sample_time},
                {"lastFlushTime",     sm.last_flush_time},
                {"wsConnections",     sm.ws_connections},
                {"dbSizeBytes",       sm.db_size_bytes},
                {"walSizeBytes",      sm.wal_size_bytes},
                {"registeredMetrics", sm.registered_metrics},
                {"serviceMode",       sm.service_mode}
            };
            res.set_content(body.dump(), "application/json");
        });

        // Config update
        svr.Post("/api/v1/config", [](const httplib::Request& req, httplib::Response& res) {
            auto origin = req.get_header_value("Origin");
            if (!origin.empty() && origin.find("127.0.0.1") == std::string::npos
                                && origin.find("localhost") == std::string::npos) {
                res.status = 403;
                res.set_content(R"({"error":"Invalid origin"})", "application/json");
                return;
            }
            // Placeholder — config updates will be implemented with the settings page
            res.set_content(R"({"status":"ok","message":"Config endpoint placeholder"})",
                            "application/json");
        });
    }

    void setup_static_serving() {
        if (!web_dir.empty()) {
            svr.set_mount_point("/", web_dir);
            spdlog::info("Serving static files from: {}", web_dir);
        }
    }
};

// ── Public API ──────────────────────────────────────────────────

HttpServer::HttpServer(MetricRegistry& registry,
                       StorageReader& reader,
                       StorageWriter& writer,
                       Database& database)
    : impl_(std::make_unique<Impl>(registry, reader, writer, database)) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::set_web_dir(const std::string& dir) {
    impl_->web_dir = dir;
}

bool HttpServer::listen(const std::string& host, int port) {
    impl_->setup_routes();

    // Cap request body to 1 MB
    impl_->svr.set_payload_max_length(1024 * 1024);

    spdlog::info("HTTP server starting on {}:{}", host, port);
    return impl_->svr.listen(host, port);
}

void HttpServer::stop() {
    if (impl_) {
        impl_->svr.stop();
    }
}

int HttpServer::ws_connection_count() const {
    return impl_->ws_connections.load();
}

void HttpServer::broadcast_delta(const std::vector<MetricSample>& /*samples*/) {
    // WebSocket broadcast will be implemented when cpp-httplib WS support
    // is integrated. For now, clients poll /api/v1/live/snapshot.
}

} // namespace pulseport
