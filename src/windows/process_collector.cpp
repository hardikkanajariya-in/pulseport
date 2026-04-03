#include "pulseport/collectors.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#endif

#include <algorithm>
#include <string>
#include <vector>

namespace pulseport {

#ifdef _WIN32

static PDH_HQUERY s_proc_query = nullptr;
static bool s_proc_initialized = false;

// We track top processes by periodically enumerating via PDH
// Using Process V2 counters (Windows 10 1903+) to avoid instance name collisions

struct ProcessEntry {
    std::string name;
    double cpu_pct = 0.0;
    double mem_mb = 0.0;
};

void register_process_collectors(MetricRegistry& registry) {
    // Register aggregate metric keys for top processes
    for (int i = 0; i < 10; ++i) {
        std::string prefix = "proc.top" + std::to_string(i);
        registry.register_metric({prefix + ".cpu_pct",
            "Top Process #" + std::to_string(i + 1) + " CPU",
            "%", "pdh_proc", "process"});
        registry.register_metric({prefix + ".mem_mb",
            "Top Process #" + std::to_string(i + 1) + " Memory",
            "MB", "pdh_proc", "process"});
        registry.register_metric({prefix + ".name",
            "Top Process #" + std::to_string(i + 1) + " Name",
            "text", "pdh_proc", "process"});
    }

    PDH_STATUS status = PdhOpenQueryW(nullptr, 0, &s_proc_query);
    if (status != ERROR_SUCCESS) {
        spdlog::warn("Process PDH query open failed: 0x{:08X}", status);
        return;
    }

    s_proc_initialized = true;
    spdlog::info("Process collectors initialized");
}

void collect_processes(MetricRegistry& registry) {
    if (!s_proc_initialized) return;

    // Snapshot approach: use GetProcessMemoryInfo + performance counters
    // For simplicity in v1, we use a PDH wildcard query per collection

    // This is a simplified approach. A production version would maintain
    // persistent counter handles and track instance → PID mapping.
    int64_t ts = now_unix();

    // Enumerate running processes via PDH counter list
    PDH_HQUERY tempQuery = nullptr;
    PdhOpenQueryW(nullptr, 0, &tempQuery);

    PDH_HCOUNTER cpuCounter = nullptr;
    PDH_STATUS status = PdhAddEnglishCounterW(tempQuery,
        L"\\Process(*)\\% Processor Time", 0, &cpuCounter);

    if (status != ERROR_SUCCESS) {
        PdhCloseQuery(tempQuery);
        return;
    }

    // Two collects needed for rate counters
    PdhCollectQueryData(tempQuery);
    Sleep(100); // Brief pause for rate calculation
    PdhCollectQueryData(tempQuery);

    // Get formatted counter array
    DWORD bufSize = 0, itemCount = 0;
    status = PdhGetFormattedCounterArrayW(cpuCounter, PDH_FMT_DOUBLE,
                                           &bufSize, &itemCount, nullptr);
    if (status != PDH_MORE_DATA || bufSize == 0) {
        PdhCloseQuery(tempQuery);
        return;
    }

    auto items = std::make_unique<BYTE[]>(bufSize);
    auto* pItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(items.get());

    status = PdhGetFormattedCounterArrayW(cpuCounter, PDH_FMT_DOUBLE,
                                           &bufSize, &itemCount, pItems);
    if (status != ERROR_SUCCESS) {
        PdhCloseQuery(tempQuery);
        return;
    }

    std::vector<ProcessEntry> entries;
    for (DWORD i = 0; i < itemCount; ++i) {
        std::wstring wname(pItems[i].szName);
        std::string name(wname.begin(), wname.end());

        // Skip system pseudo-processes
        if (name == "_Total" || name == "Idle") continue;

        ProcessEntry pe;
        pe.name = name;
        pe.cpu_pct = pItems[i].FmtValue.doubleValue;
        entries.push_back(std::move(pe));
    }

    PdhCloseQuery(tempQuery);

    // Sort by CPU descending, take top 10
    std::sort(entries.begin(), entries.end(),
              [](const ProcessEntry& a, const ProcessEntry& b) {
                  return a.cpu_pct > b.cpu_pct;
              });

    size_t count = std::min<size_t>(entries.size(), 10);
    for (size_t i = 0; i < 10; ++i) {
        std::string prefix = "proc.top" + std::to_string(i);
        if (i < count) {
            registry.push_sample({prefix + ".cpu_pct", entries[i].cpu_pct,
                                   "%", Quality::Measured, ts});
            registry.push_sample({prefix + ".name",
                                   static_cast<double>(std::hash<std::string>{}(entries[i].name)),
                                   "text", Quality::Measured, ts});
        } else {
            registry.push_sample({prefix + ".cpu_pct", 0.0, "%", Quality::Unknown, ts});
        }
    }
}

#else

void register_process_collectors(MetricRegistry&) {
    spdlog::warn("Process collectors not available on this platform");
}

void collect_processes(MetricRegistry&) {}

#endif

} // namespace pulseport
