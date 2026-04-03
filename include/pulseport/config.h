#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>

namespace pulseport {

/// Application configuration loaded from config.json.
struct Config {
    std::string host = "127.0.0.1";
    int         port = 9770;
    std::string log_level = "info";
    std::string db_path;       // Resolved at runtime
    std::string web_dir;       // Resolved at runtime
    std::string migrations_dir; // Resolved at runtime
    std::string log_dir;       // Resolved at runtime

    // Retention
    int retention_1m_days    = 90;
    int retention_15m_days   = 365;
    int retention_daily_days = 365;
    int retention_events_days = 365;

    // Sampling
    int sample_interval_ms    = 1000;
    int process_interval_ms   = 5000;
    int thermal_interval_ms   = 5000;
    int aggregation_interval_s = 60;

    // Alert thresholds
    double alert_cpu_high_pct      = 90.0;
    int    alert_cpu_sustained_min = 5;
    double alert_mem_high_pct      = 90.0;
    int    alert_mem_sustained_min = 5;
    double alert_battery_low_pct   = 15.0;
    double alert_power_high_w      = 100.0;
    int    alert_cooldown_minutes  = 30;
};

/// Load config from a JSON file. Missing fields use defaults.
Config load_config(const std::filesystem::path& path);

/// Save config to a JSON file.
void save_config(const Config& cfg, const std::filesystem::path& path);

/// Resolve runtime paths (ProgramData, exe-relative, etc.).
Config resolve_paths(Config cfg);

/// JSON conversion.
void to_json(nlohmann::json& j, const Config& c);
void from_json(const nlohmann::json& j, Config& c);

} // namespace pulseport
