#pragma once

#include "pulseport/metric_registry.h"
#include "pulseport/database.h"
#include "pulseport/storage.h"
#include "pulseport/config.h"
#include "pulseport/power_pipeline.h"

#include <atomic>
#include <memory>
#include <string>
#include <functional>

namespace pulseport {

/// HTTP + WebSocket server bound to 127.0.0.1.
/// Serves the dashboard frontend, REST API, and live telemetry stream.
class HttpServer {
public:
    HttpServer(MetricRegistry& registry,
               StorageReader& reader,
               StorageWriter& writer,
               Database& database);
    ~HttpServer();

    /// Start listening on the given port. Blocks until stop() is called.
    bool listen(const std::string& host, int port);

    /// Signal the server to stop. Thread-safe.
    void stop();

    /// Set the directory to serve static frontend files from.
    void set_web_dir(const std::string& dir);

    /// Get count of active WebSocket connections.
    int ws_connection_count() const;

    /// Broadcast a delta message to all connected WebSocket clients.
    void broadcast_delta(const std::vector<MetricSample>& samples);

    /// Broadcast an event notification to all connected WebSocket clients.
    void broadcast_event(const std::string& severity, const std::string& category,
                         const std::string& title);

    /// Set the PowerPipeline reference for rolling averages.
    void set_power_pipeline(PowerPipeline* pipeline);

    /// Set mutable config reference for config POST endpoint.
    void set_config(Config* cfg, const std::string& config_path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pulseport
