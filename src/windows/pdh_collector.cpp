#include "pulseport/collectors.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#endif

namespace pulseport {

#ifdef _WIN32

// ── PDH query handles (module-level, initialized once) ──────────

static PDH_HQUERY  s_query = nullptr;
static PDH_HCOUNTER s_cpu_total = nullptr;
static PDH_HCOUNTER s_mem_available = nullptr;
static PDH_HCOUNTER s_disk_time = nullptr;
static PDH_HCOUNTER s_disk_read = nullptr;
static PDH_HCOUNTER s_disk_write = nullptr;
static PDH_HCOUNTER s_net_recv = nullptr;
static PDH_HCOUNTER s_net_send = nullptr;

void register_pdh_collectors(MetricRegistry& registry) {
    // Register metric metadata
    registry.register_metric({"cpu.total_pct", "CPU Total", "%", "pdh", "cpu"});
    registry.register_metric({"mem.available_mb", "Memory Available", "MB", "pdh", "memory"});
    registry.register_metric({"mem.used_pct", "Memory Used", "%", "computed", "memory"});
    registry.register_metric({"disk.active_pct", "Disk Active Time", "%", "pdh", "disk"});
    registry.register_metric({"disk.read_bps", "Disk Read", "B/s", "pdh", "disk"});
    registry.register_metric({"disk.write_bps", "Disk Write", "B/s", "pdh", "disk"});
    registry.register_metric({"net.recv_bps", "Network Receive", "B/s", "pdh", "network"});
    registry.register_metric({"net.send_bps", "Network Send", "B/s", "pdh", "network"});

    // Open PDH query
    PDH_STATUS status = PdhOpenQueryW(nullptr, 0, &s_query);
    if (status != ERROR_SUCCESS) {
        spdlog::error("PdhOpenQuery failed: 0x{:08X}", status);
        return;
    }

    // Add counters
    auto add_counter = [](const wchar_t* path, PDH_HCOUNTER* counter) {
        PDH_STATUS s = PdhAddEnglishCounterW(s_query, path, 0, counter);
        if (s != ERROR_SUCCESS) {
            spdlog::warn("PdhAddCounter failed for {}: 0x{:08X}",
                         std::string(path, path + wcslen(path)), s);
        }
    };

    add_counter(L"\\Processor(_Total)\\% Processor Time", &s_cpu_total);
    add_counter(L"\\Memory\\Available MBytes", &s_mem_available);
    add_counter(L"\\PhysicalDisk(_Total)\\% Disk Time", &s_disk_time);
    add_counter(L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", &s_disk_read);
    add_counter(L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", &s_disk_write);
    add_counter(L"\\Network Interface(*)\\Bytes Received/sec", &s_net_recv);
    add_counter(L"\\Network Interface(*)\\Bytes Sent/sec", &s_net_send);

    // Initial collect to prime counters (first value is always 0 for rate counters)
    PdhCollectQueryData(s_query);
    spdlog::info("PDH collectors initialized");
}

void collect_pdh(MetricRegistry& registry) {
    if (!s_query) return;

    PDH_STATUS status = PdhCollectQueryData(s_query);
    if (status != ERROR_SUCCESS) {
        spdlog::debug("PdhCollectQueryData failed: 0x{:08X}", status);
        return;
    }

    int64_t ts = now_unix();
    PDH_FMT_COUNTERVALUE val;

    auto read_double = [&](PDH_HCOUNTER counter, const char* key,
                           const char* unit) {
        if (!counter) return;
        status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &val);
        if (status == ERROR_SUCCESS) {
            registry.push_sample({key, val.doubleValue, unit, Quality::Measured, ts});
        }
    };

    read_double(s_cpu_total, "cpu.total_pct", "%");
    read_double(s_mem_available, "mem.available_mb", "MB");
    read_double(s_disk_time, "disk.active_pct", "%");
    read_double(s_disk_read, "disk.read_bps", "B/s");
    read_double(s_disk_write, "disk.write_bps", "B/s");
    read_double(s_net_recv, "net.recv_bps", "B/s");
    read_double(s_net_send, "net.send_bps", "B/s");

    // Compute memory used percentage
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        double used_pct = static_cast<double>(memInfo.dwMemoryLoad);
        registry.push_sample({"mem.used_pct", used_pct, "%", Quality::Measured, ts});
    }
}

#else

void register_pdh_collectors(MetricRegistry&) {
    spdlog::warn("PDH collectors not available on this platform");
}

void collect_pdh(MetricRegistry&) {}

#endif

} // namespace pulseport
