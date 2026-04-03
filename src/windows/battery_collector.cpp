#include "pulseport/collectors.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <batclass.h>
#include <poclass.h>
#include <setupapi.h>
#include <devguid.h>
#pragma comment(lib, "setupapi.lib")
#endif

namespace pulseport {

#ifdef _WIN32

// Attempt to read battery discharge rate via IOCTL.
// Returns watts if successful, or -1.0 on failure.
static double query_battery_rate_ioctl() {
    HDEVINFO hdev = SetupDiGetClassDevsW(
        &GUID_DEVCLASS_BATTERY, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev == INVALID_HANDLE_VALUE) return -1.0;

    SP_DEVICE_INTERFACE_DATA iface_data{};
    iface_data.cbSize = sizeof(iface_data);

    if (!SetupDiEnumDeviceInterfaces(hdev, nullptr, &GUID_DEVCLASS_BATTERY,
                                      0, &iface_data)) {
        SetupDiDestroyDeviceInfoList(hdev);
        return -1.0;
    }

    DWORD required_size = 0;
    SetupDiGetDeviceInterfaceDetailW(hdev, &iface_data, nullptr, 0,
                                      &required_size, nullptr);
    if (required_size == 0) {
        SetupDiDestroyDeviceInfoList(hdev);
        return -1.0;
    }

    auto detail_buf = std::make_unique<char[]>(required_size);
    auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_buf.get());
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    if (!SetupDiGetDeviceInterfaceDetailW(hdev, &iface_data, detail,
                                           required_size, nullptr, nullptr)) {
        SetupDiDestroyDeviceInfoList(hdev);
        return -1.0;
    }

    HANDLE hBattery = CreateFileW(detail->DevicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    SetupDiDestroyDeviceInfoList(hdev);

    if (hBattery == INVALID_HANDLE_VALUE) return -1.0;

    // Get battery tag
    ULONG battery_tag = 0;
    ULONG wait = 0;
    DWORD bytes_returned = 0;
    if (!DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_TAG,
                          &wait, sizeof(wait),
                          &battery_tag, sizeof(battery_tag),
                          &bytes_returned, nullptr) || battery_tag == 0) {
        CloseHandle(hBattery);
        return -1.0;
    }

    // Query battery status
    BATTERY_WAIT_STATUS bws{};
    bws.BatteryTag = battery_tag;
    BATTERY_STATUS bs{};

    if (!DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_STATUS,
                          &bws, sizeof(bws),
                          &bs, sizeof(bs),
                          &bytes_returned, nullptr)) {
        CloseHandle(hBattery);
        return -1.0;
    }

    CloseHandle(hBattery);

    // Rate is in mW (negative = discharging). BATTERY_UNKNOWN_RATE = 0x80000000
    if (bs.Rate == static_cast<LONG>(BATTERY_UNKNOWN_RATE) || bs.Rate == 0) {
        return -1.0;
    }

    return std::abs(static_cast<double>(bs.Rate)) / 1000.0; // mW → W
}

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

    bool ac_online = (sps.ACLineStatus == 1);
    registry.push_sample({"battery.ac_online", ac_online ? 1.0 : 0.0,
                           "bool", Quality::Measured, ts});

    if (sps.BatteryLifePercent != 255) {
        double level = static_cast<double>(sps.BatteryLifePercent);
        registry.push_sample({"battery.level_pct", level, "%", Quality::Measured, ts});
    }

    bool charging = (sps.BatteryFlag & 8) != 0;
    registry.push_sample({"battery.charging", charging ? 1.0 : 0.0,
                           "bool", Quality::Measured, ts});

    // Time remaining (only meaningful on battery)
    if (!ac_online && sps.BatteryLifeTime != static_cast<DWORD>(-1)) {
        double remaining_min = static_cast<double>(sps.BatteryLifeTime) / 60.0;
        registry.push_sample({"battery.remaining_min", remaining_min,
                               "min", Quality::Derived, ts});
    }

    // Power draw via IOCTL (precise) with fallback
    double watts = query_battery_rate_ioctl();
    if (watts >= 0.0) {
        registry.push_sample({"power.current_w", watts, "W", Quality::Measured, ts});
    } else if (!ac_online && sps.BatteryLifePercent != 255
               && sps.BatteryLifeTime != static_cast<DWORD>(-1)
               && sps.BatteryLifeTime > 0) {
        // Rough estimate: assume ~50Wh typical battery
        double hours_remaining = static_cast<double>(sps.BatteryLifeTime) / 3600.0;
        double pct = static_cast<double>(sps.BatteryLifePercent) / 100.0;
        constexpr double kTypicalBatteryWh = 50.0;
        double est_watts = (kTypicalBatteryWh * pct) / hours_remaining;
        registry.push_sample({"power.current_w", est_watts, "W", Quality::Estimated, ts});
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
