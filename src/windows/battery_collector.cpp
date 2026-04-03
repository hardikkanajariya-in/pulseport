#include "pulseport/collectors.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <batclass.h>
#include <setupapi.h>
#include <devguid.h>
#pragma comment(lib, "setupapi.lib")
#endif

namespace pulseport {

#ifdef _WIN32

void register_battery_collectors(MetricRegistry& registry) {
    registry.register_metric({"battery.level_pct", "Battery Level", "%", "system", "battery"});
    registry.register_metric({"battery.charging", "Charging", "bool", "system", "battery"});
    registry.register_metric({"battery.ac_online", "AC Power", "bool", "system", "battery"});
    registry.register_metric({"power.current_w", "Power Draw", "W", "battery_rate", "power"});
    registry.register_metric({"battery.remaining_min", "Time Remaining", "min", "system", "battery"});
    spdlog::info("Battery collectors registered");
}

void collect_battery(MetricRegistry& registry) {
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) {
        spdlog::debug("GetSystemPowerStatus failed");
        return;
    }

    int64_t ts = now_unix();

    // AC status
    bool ac_online = (sps.ACLineStatus == 1);
    registry.push_sample({"battery.ac_online", ac_online ? 1.0 : 0.0,
                           "bool", Quality::Measured, ts});

    // Battery level
    if (sps.BatteryLifePercent != 255) {
        double level = static_cast<double>(sps.BatteryLifePercent);
        registry.push_sample({"battery.level_pct", level, "%", Quality::Measured, ts});
    }

    // Charging status
    bool charging = (sps.BatteryFlag & 8) != 0; // Bit 3 = charging
    registry.push_sample({"battery.charging", charging ? 1.0 : 0.0,
                           "bool", Quality::Measured, ts});

    // Time remaining (only meaningful on battery)
    if (!ac_online && sps.BatteryLifeTime != static_cast<DWORD>(-1)) {
        double remaining_min = static_cast<double>(sps.BatteryLifeTime) / 60.0;
        registry.push_sample({"battery.remaining_min", remaining_min,
                               "min", Quality::Derived, ts});
    }

    // Power draw estimation from battery rate
    // Note: GetSystemPowerStatus does not provide wattage directly.
    // For derived power, we'd need battery capacity and discharge rate
    // from IOCTL_BATTERY_STATUS. For now, mark as unknown when on AC.
    if (!ac_online && sps.BatteryLifePercent != 255) {
        // Placeholder: real implementation will use IOCTL_BATTERY_STATUS
        // to get BatteryRate (in mW, negative = discharging)
        registry.push_sample({"power.current_w", 0.0, "W", Quality::Unknown, ts});
    } else {
        registry.push_sample({"power.current_w", 0.0, "W", Quality::Unknown, ts});
    }
}

#else

void register_battery_collectors(MetricRegistry&) {
    spdlog::warn("Battery collectors not available on this platform");
}

void collect_battery(MetricRegistry&) {}

#endif

} // namespace pulseport
