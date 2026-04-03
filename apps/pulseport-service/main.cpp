#include "pulseport/config.h"
#include "pulseport/database.h"
#include "pulseport/http_server.h"
#include "pulseport/metric_registry.h"
#include "pulseport/sampler.h"
#include "pulseport/aggregator.h"
#include "pulseport/collectors.h"
#include "pulseport/self_metrics.h"
#include "pulseport/service_control.h"
#include "pulseport/storage.h"
#include "pulseport/version.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef _WIN32
#include <spdlog/sinks/win_eventlog_sink.h>
#endif

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static void setup_logging(const pulseport::Config& cfg, bool console_mode) {
    std::vector<spdlog::sink_ptr> sinks;

    // Rotating file sink
    if (!cfg.log_dir.empty()) {
        fs::create_directories(cfg.log_dir);
        auto file_path = (fs::path(cfg.log_dir) / "pulseport.log").string();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file_path, 5 * 1024 * 1024, 3);
        sinks.push_back(file_sink);
    }

    // Console sink (only in console mode)
    if (console_mode) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);
    }

#ifdef _WIN32
    // Windows Event Log sink (critical only)
    auto event_sink = std::make_shared<spdlog::sinks::win_eventlog_sink_mt>("PulsePort");
    event_sink->set_level(spdlog::level::critical);
    sinks.push_back(event_sink);
#endif

    auto logger = std::make_shared<spdlog::logger>("", sinks.begin(), sinks.end());
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");

    // Set log level from config
    if (cfg.log_level == "trace")    logger->set_level(spdlog::level::trace);
    else if (cfg.log_level == "debug") logger->set_level(spdlog::level::debug);
    else if (cfg.log_level == "warn")  logger->set_level(spdlog::level::warn);
    else if (cfg.log_level == "error") logger->set_level(spdlog::level::err);
    else logger->set_level(spdlog::level::info);

    logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(3));
}

static void run_application(const pulseport::Config& cfg) {
    using namespace pulseport;

    spdlog::info("PulsePort {} starting", PULSEPORT_VERSION);

    // Initialize self-metrics
    auto& sm = self_metrics();
    sm.version = PULSEPORT_VERSION;
    sm.start_time_unix = now_unix();

    // Open database
    Database database;
    if (!database.open(cfg.db_path, cfg.migrations_dir)) {
        spdlog::critical("Failed to open database at {}", cfg.db_path);
        return;
    }

    // Storage layers
    StorageWriter writer(database.handle());
    StorageReader reader(database.handle());

    // Metric registry
    MetricRegistry registry;

    // Register collectors
    register_pdh_collectors(registry);
    register_battery_collectors(registry);
    register_thermal_collectors(registry);
    register_process_collectors(registry);

    // Persist metric registry to DB
    for (const auto& info : registry.all_metrics()) {
        writer.write_metric_info(info);
    }

    // Aggregator
    Aggregator aggregator(registry, writer);

    // Sampler
    Sampler sampler(registry);

    // Add collector callbacks
    sampler.add_collector("pdh", cfg.sample_interval_ms,
        [](MetricRegistry& r) { collect_pdh(r); });

    sampler.add_collector("battery", cfg.sample_interval_ms,
        [](MetricRegistry& r) { collect_battery(r); });

    sampler.add_collector("thermal", cfg.thermal_interval_ms,
        [](MetricRegistry& r) { collect_thermal(r); });

    sampler.add_collector("processes", cfg.process_interval_ms,
        [](MetricRegistry& r) { collect_processes(r); });

    // Aggregation callback (runs every 60s via sampler)
    sampler.add_collector("aggregator_flush", cfg.aggregation_interval_s * 1000,
        [&aggregator](MetricRegistry&) { aggregator.flush_1m(); });

    // Log service start event
    writer.write_event(now_unix(), "info", "lifecycle", "PulsePort service started");

    // Start sampler
    sampler.start();

    // HTTP server (runs on its own thread)
    HttpServer server(registry, reader, writer, database);
    server.set_web_dir(cfg.web_dir);

    std::jthread server_thread([&server, &cfg]() {
        if (!server.listen(cfg.host, cfg.port)) {
            spdlog::error("HTTP server failed to start on {}:{}", cfg.host, cfg.port);
            service::signal_stop();
        }
    });

    spdlog::info("PulsePort running on http://{}:{}", cfg.host, cfg.port);

    // Wait for shutdown signal
    service::wait_for_stop();

    spdlog::info("Shutdown initiated");

    // Stop in reverse order
    server.stop();
    sampler.stop();

    // Final flush
    aggregator.flush_1m();

    // Log shutdown event
    writer.write_event(now_unix(), "info", "lifecycle", "PulsePort service stopped");

    // Database close (includes checkpoint)
    database.close();

    spdlog::info("PulsePort stopped cleanly");
}

int main(int argc, char* argv[]) {
    using namespace pulseport;

    // Parse command-line arguments
    bool console_mode = false;
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--console" || arg == "-c") {
            console_mode = true;
        } else if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "PulsePort " << PULSEPORT_VERSION << std::endl;
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "PulsePort - Local Observability Service\n"
                      << "Usage: pulseport-service [options]\n\n"
                      << "Options:\n"
                      << "  --console, -c     Run in foreground (console mode)\n"
                      << "  --service         Run as Windows service (default)\n"
                      << "  --config <path>   Path to config.json\n"
                      << "  --version, -v     Show version\n"
                      << "  --help, -h        Show this help\n";
            return 0;
        }
    }

    // Load config
    Config cfg;
    if (!config_path.empty()) {
        cfg = load_config(config_path);
    } else {
        // Try default locations
        cfg = load_config("config.json");
    }
    cfg = resolve_paths(cfg);

    // Setup logging
    setup_logging(cfg, console_mode);
    self_metrics().service_mode = console_mode ? "console" : "service";

    auto main_fn = [&cfg]() {
        run_application(cfg);
    };

    if (console_mode) {
        service::run_as_console(main_fn);
    } else {
        if (!service::run_as_service("PulsePort", main_fn)) {
            // If SCM dispatch failed, fall back to console mode
            spdlog::info("Falling back to console mode");
            service::run_as_console(main_fn);
        }
    }

    return 0