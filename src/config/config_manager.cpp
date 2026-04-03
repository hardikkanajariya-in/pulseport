#include "pulseport/config.h"
#include <spdlog/spdlog.h>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#endif

namespace pulseport {

void to_json(nlohmann::json& j, const Config& c) {
    j = {
        {"host",                 c.host},
        {"port",                 c.port},
        {"log_level",            c.log_level},
        {"retention_1m_days",    c.retention_1m_days},
        {"retention_15m_days",   c.retention_15m_days},
        {"retention_daily_days", c.retention_daily_days},
        {"retention_events_days", c.retention_events_days},
        {"sample_interval_ms",   c.sample_interval_ms},
        {"process_interval_ms",  c.process_interval_ms},
        {"thermal_interval_ms",  c.thermal_interval_ms},
        {"aggregation_interval_s", c.aggregation_interval_s},
        {"alert_cpu_high_pct",      c.alert_cpu_high_pct},
        {"alert_cpu_sustained_min", c.alert_cpu_sustained_min},
        {"alert_mem_high_pct",      c.alert_mem_high_pct},
        {"alert_mem_sustained_min", c.alert_mem_sustained_min},
        {"alert_battery_low_pct",   c.alert_battery_low_pct},
        {"alert_power_high_w",      c.alert_power_high_w},
        {"alert_cooldown_minutes",  c.alert_cooldown_minutes}
    };
}

void from_json(const nlohmann::json& j, Config& c) {
    if (j.contains("host"))                  j.at("host").get_to(c.host);
    if (j.contains("port"))                  j.at("port").get_to(c.port);
    if (j.contains("log_level"))             j.at("log_level").get_to(c.log_level);
    if (j.contains("retention_1m_days"))     j.at("retention_1m_days").get_to(c.retention_1m_days);
    if (j.contains("retention_15m_days"))    j.at("retention_15m_days").get_to(c.retention_15m_days);
    if (j.contains("retention_daily_days"))  j.at("retention_daily_days").get_to(c.retention_daily_days);
    if (j.contains("retention_events_days")) j.at("retention_events_days").get_to(c.retention_events_days);
    if (j.contains("sample_interval_ms"))    j.at("sample_interval_ms").get_to(c.sample_interval_ms);
    if (j.contains("process_interval_ms"))   j.at("process_interval_ms").get_to(c.process_interval_ms);
    if (j.contains("thermal_interval_ms"))   j.at("thermal_interval_ms").get_to(c.thermal_interval_ms);
    if (j.contains("aggregation_interval_s")) j.at("aggregation_interval_s").get_to(c.aggregation_interval_s);
    if (j.contains("alert_cpu_high_pct"))      j.at("alert_cpu_high_pct").get_to(c.alert_cpu_high_pct);
    if (j.contains("alert_cpu_sustained_min")) j.at("alert_cpu_sustained_min").get_to(c.alert_cpu_sustained_min);
    if (j.contains("alert_mem_high_pct"))      j.at("alert_mem_high_pct").get_to(c.alert_mem_high_pct);
    if (j.contains("alert_mem_sustained_min")) j.at("alert_mem_sustained_min").get_to(c.alert_mem_sustained_min);
    if (j.contains("alert_battery_low_pct"))   j.at("alert_battery_low_pct").get_to(c.alert_battery_low_pct);
    if (j.contains("alert_power_high_w"))      j.at("alert_power_high_w").get_to(c.alert_power_high_w);
    if (j.contains("alert_cooldown_minutes"))  j.at("alert_cooldown_minutes").get_to(c.alert_cooldown_minutes);
}

Config load_config(const std::filesystem::path& path) {
    Config cfg;
    if (!std::filesystem::exists(path)) {
        spdlog::info("No config file at {}, using defaults", path.string());
        return cfg;
    }

    try {
        std::ifstream file(path);
        nlohmann::json j = nlohmann::json::parse(file);
        cfg = j.get<Config>();
        spdlog::info("Loaded config from {}", path.string());
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse config {}: {}", path.string(), e.what());
    }
    return cfg;
}

void save_config(const Config& cfg, const std::filesystem::path& path) {
    try {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        nlohmann::json j = cfg;
        file << j.dump(2);
        spdlog::info("Saved config to {}", path.string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config: {}", e.what());
    }
}

Config resolve_paths(Config cfg) {
    namespace fs = std::filesystem;

#ifdef _WIN32
    // Use ProgramData for data files when running as service
    wchar_t pdata[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, pdata))) {
        fs::path base = fs::path(pdata) / "PulsePort";
        if (cfg.db_path.empty())
            cfg.db_path = (base / "pulseport.db").string();
        if (cfg.log_dir.empty())
            cfg.log_dir = (base / "logs").string();
    }
#endif

    // Resolve web_dir and migrations_dir relative to executable
    if (cfg.web_dir.empty() || cfg.migrations_dir.empty()) {
#ifdef _WIN32
        wchar_t exe_path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        fs::path exe_dir = fs::path(exe_path).parent_path();
#else
        fs::path exe_dir = fs::current_path();
#endif
        if (cfg.web_dir.empty())
            cfg.web_dir = (exe_dir / "web").string();
        if (cfg.migrations_dir.empty())
            cfg.migrations_dir = (exe_dir / "db" / "migrations").string();
    }

    return cfg;
}

} // namespace pulseport
