#include "pulseport/http_server.h"
#include "pulseport/self_metrics.h"
#include "pulseport/version.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define closesocket close
#endif

using json = nlohmann::json;

namespace pulseport {

// Base64 encoding

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(4 * ((len + 2) / 3));
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= data[i + 2];
        out.push_back(kBase64Chars[(n >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? kBase64Chars[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? kBase64Chars[n & 0x3F] : '=');
    }
    return out;
}

// Minimal SHA-1 implementation for WebSocket accept key
static void sha1(const uint8_t* msg, size_t len, uint8_t out[20]) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE,
             h3 = 0x10325476, h4 = 0xC3D2E1F0;

    size_t new_len = len + 1;
    while (new_len % 64 != 56) ++new_len;
    std::vector<uint8_t> buf(new_len + 8, 0);
    memcpy(buf.data(), msg, len);
    buf[len] = 0x80;
    uint64_t bits = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i)
        buf[new_len + i] = static_cast<uint8_t>(bits >> (56 - 8 * i));

    auto left_rotate = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };

    for (size_t offset = 0; offset < buf.size(); offset += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (buf[offset + 4 * i] << 24) | (buf[offset + 4 * i + 1] << 16) |
                   (buf[offset + 4 * i + 2] << 8) | buf[offset + 4 * i + 3];
        for (int i = 16; i < 80; ++i)
            w[i] = left_rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else              { f = b ^ c ^ d;                    k = 0xCA62C1D6; }
            uint32_t tmp = left_rotate(a, 5) + f + e + k + w[i];
            e = d; d = c; c = left_rotate(b, 30); b = a; a = tmp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    for (int i = 0; i < 4; ++i) {
        out[i]      = static_cast<uint8_t>(h0 >> (24 - 8 * i));
        out[4 + i]  = static_cast<uint8_t>(h1 >> (24 - 8 * i));
        out[8 + i]  = static_cast<uint8_t>(h2 >> (24 - 8 * i));
        out[12 + i] = static_cast<uint8_t>(h3 >> (24 - 8 * i));
        out[16 + i] = static_cast<uint8_t>(h4 >> (24 - 8 * i));
    }
}

static std::string ws_accept_key(const std::string& client_key) {
    std::string combined = client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t hash[20];
    sha1(reinterpret_cast<const uint8_t*>(combined.data()), combined.size(), hash);
    return base64_encode(hash, 20);
}

// WebSocket frame helpers

static bool ws_send_text(SOCKET sock, const std::string& msg) {
    size_t len = msg.size();
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + TEXT opcode
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<uint8_t>((len >> (8 * i)) & 0xFF));
    }
    frame.insert(frame.end(), msg.begin(), msg.end());
    int sent = send(sock, reinterpret_cast<const char*>(frame.data()),
                     static_cast<int>(frame.size()), 0);
    return sent > 0;
}

static void ws_send_ping(SOCKET sock) {
    uint8_t frame[] = {0x89, 0x00}; // PING, no payload
    send(sock, reinterpret_cast<const char*>(frame), 2, 0);
}

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

struct WsClient {
    SOCKET sock = INVALID_SOCKET;
    std::mutex send_mutex;
    std::atomic<bool> alive{true};
    std::jthread reader_thread;

    WsClient() = default;
    explicit WsClient(SOCKET s) : sock(s) {}
    WsClient(const WsClient&) = delete;
    WsClient& operator=(const WsClient&) = delete;
};

struct HttpServer::Impl {
    httplib::Server svr;
    MetricRegistry& registry;
    StorageReader&  reader;
    StorageWriter&  writer;
    Database&       database;
    std::string     web_dir;
    PowerPipeline*  power_pipeline = nullptr;
    Config*         config_ptr = nullptr;
    std::string     config_path;

    std::mutex ws_mutex;
    std::vector<std::shared_ptr<WsClient>> ws_clients;

    Impl(MetricRegistry& r, StorageReader& rd, StorageWriter& wr, Database& db)
        : registry(r), reader(rd), writer(wr), database(db) {}

    int ws_count() const {
        return static_cast<int>(ws_clients.size());
    }

    void remove_dead_clients() {
        std::lock_guard lock(ws_mutex);
        ws_clients.erase(
            std::remove_if(ws_clients.begin(), ws_clients.end(),
                [](const auto& c) { return !c->alive.load(); }),
            ws_clients.end());
    }

    void broadcast(const std::string& msg) {
        std::lock_guard lock(ws_mutex);
        for (auto& client : ws_clients) {
            if (client->alive.load()) {
                std::lock_guard slock(client->send_mutex);
                if (!ws_send_text(client->sock, msg)) {
                    client->alive.store(false);
                }
            }
        }
    }

    void handle_ws_upgrade(const httplib::Request& req, httplib::Response& res) {
        auto ws_key = req.get_header_value("Sec-WebSocket-Key");
        if (ws_key.empty()) {
            res.status = 400;
            return;
        }

        // cpp-httplib doesn't expose the socket directly for upgrade
        std::string accept = ws_accept_key(ws_key);
        res.status = 101;
        res.set_header("Upgrade", "websocket");
        res.set_header("Connection", "Upgrade");
        res.set_header("Sec-WebSocket-Accept", accept);
    }

    void setup_routes() {
        setup_api_routes();
        setup_static_serving();
    }

    void setup_api_routes() {
        svr.Get("/api/v1/health", [this](const httplib::Request&, httplib::Response& res) {
            auto& sm = self_metrics();
            json body = {
                {"status",  "ok"},
                {"version", PULSEPORT_VERSION},
                {"uptime",  sm.uptime_seconds()}
            };
            res.set_content(body.dump(), "application/json");
        });

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

        svr.Get("/api/v1/live/snapshot", [this](const httplib::Request&, httplib::Response& res) {
            auto snap = registry.snapshot();
            json metrics = json::array();
            for (const auto& s : snap) {
                metrics.push_back(sample_to_json(s));
            }
            if (power_pipeline) {
                int64_t ts = now_unix();
                auto q = power_pipeline->current_quality();
                metrics.push_back(sample_to_json(
                    {"power.avg_1m_w", power_pipeline->avg_watts(60), "W", q, ts}));
                metrics.push_back(sample_to_json(
                    {"power.avg_5m_w", power_pipeline->avg_watts(300), "W", q, ts}));
                metrics.push_back(sample_to_json(
                    {"power.avg_15m_w", power_pipeline->avg_watts(900), "W", q, ts}));
            }
            json body = {
                {"type", "snapshot"},
                {"tsUtc", now_unix()},
                {"metrics", metrics}
            };
            res.set_content(body.dump(), "application/json");
        });

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

        svr.Post("/api/v1/history/delete", [this](const httplib::Request& req, httplib::Response& res) {
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

        svr.Get("/api/v1/diagnostics", [this](const httplib::Request&, httplib::Response& res) {
            auto& sm = self_metrics();
            sm.db_size_bytes  = database.file_size_bytes();
            sm.wal_size_bytes = database.wal_size_bytes();
            sm.ws_connections = ws_count();
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

        svr.Post("/api/v1/config", [this](const httplib::Request& req, httplib::Response& res) {
            auto origin = req.get_header_value("Origin");
            if (!origin.empty() && origin.find("127.0.0.1") == std::string::npos
                                && origin.find("localhost") == std::string::npos) {
                res.status = 403;
                res.set_content(R"({"error":"Invalid origin"})", "application/json");
                return;
            }

            if (!config_ptr) {
                res.status = 503;
                res.set_content(R"({"error":"Config not available"})", "application/json");
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

            auto apply_double = [&](const char* key, double& target, double min_v, double max_v) {
                if (body.contains(key)) {
                    double v = body[key].get<double>();
                    if (v >= min_v && v <= max_v) target = v;
                }
            };
            auto apply_int = [&](const char* key, int& target, int min_v, int max_v) {
                if (body.contains(key)) {
                    int v = body[key].get<int>();
                    if (v >= min_v && v <= max_v) target = v;
                }
            };

            apply_double("alert_cpu_high_pct", config_ptr->alert_cpu_high_pct, 1.0, 100.0);
            apply_int("alert_cpu_sustained_min", config_ptr->alert_cpu_sustained_min, 1, 60);
            apply_double("alert_mem_high_pct", config_ptr->alert_mem_high_pct, 1.0, 100.0);
            apply_int("alert_mem_sustained_min", config_ptr->alert_mem_sustained_min, 1, 60);
            apply_double("alert_battery_low_pct", config_ptr->alert_battery_low_pct, 1.0, 50.0);
            apply_double("alert_power_high_w", config_ptr->alert_power_high_w, 1.0, 1000.0);
            apply_int("alert_cooldown_minutes", config_ptr->alert_cooldown_minutes, 1, 1440);
            apply_int("retention_1m_days", config_ptr->retention_1m_days, 1, 3650);
            apply_int("retention_15m_days", config_ptr->retention_15m_days, 1, 3650);
            apply_int("retention_daily_days", config_ptr->retention_daily_days, 1, 3650);
            apply_int("retention_events_days", config_ptr->retention_events_days, 1, 3650);

            if (!config_path.empty()) {
                save_config(*config_ptr, config_path);
            }

            json result;
            to_json(result, *config_ptr);
            result["status"] = "ok";
            res.set_content(result.dump(), "application/json");
        });

        svr.Get("/api/v1/config", [this](const httplib::Request&, httplib::Response& res) {
            if (!config_ptr) {
                res.status = 503;
                res.set_content(R"({"error":"Config not available"})", "application/json");
                return;
            }
            json result;
            to_json(result, *config_ptr);
            res.set_content(result.dump(), "application/json");
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

void HttpServer::set_power_pipeline(PowerPipeline* pipeline) {
    impl_->power_pipeline = pipeline;
}

void HttpServer::set_config(Config* cfg, const std::string& config_path) {
    impl_->config_ptr = cfg;
    impl_->config_path = config_path;
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
        // Close all WebSocket clients
        {
            std::lock_guard lock(impl_->ws_mutex);
            for (auto& c : impl_->ws_clients) {
                c->alive.store(false);
                closesocket(c->sock);
            }
            impl_->ws_clients.clear();
        }
        impl_->svr.stop();
    }
}

int HttpServer::ws_connection_count() const {
    return impl_->ws_count();
}

void HttpServer::broadcast_delta(const std::vector<MetricSample>& samples) {
    if (samples.empty()) return;

    impl_->remove_dead_clients();
    if (impl_->ws_count() == 0) return;

    json metrics = json::array();
    for (const auto& s : samples) {
        metrics.push_back(sample_to_json(s));
    }
    // Append rolling power averages
    if (impl_->power_pipeline) {
        int64_t ts = now_unix();
        auto q = impl_->power_pipeline->current_quality();
        metrics.push_back(sample_to_json(
            {"power.avg_1m_w", impl_->power_pipeline->avg_watts(60), "W", q, ts}));
        metrics.push_back(sample_to_json(
            {"power.avg_5m_w", impl_->power_pipeline->avg_watts(300), "W", q, ts}));
        metrics.push_back(sample_to_json(
            {"power.avg_15m_w", impl_->power_pipeline->avg_watts(900), "W", q, ts}));
    }
    json msg = {{"type", "delta"}, {"metrics", metrics}};
    impl_->broadcast(msg.dump());
}

void HttpServer::broadcast_event(const std::string& severity,
                                  const std::string& category,
                                  const std::string& title) {
    json msg = {
        {"type", "event"},
        {"event", {
            {"severity", severity},
            {"category", category},
            {"title", title},
            {"ts", now_unix()}
        }}
    };
    impl_->broadcast(msg.dump());
}

} // namespace pulseport
