# Collectors Reference

PulsePort uses four collector modules to gather system telemetry. Each collector registers its metric keys at startup and produces samples on a timer-driven cadence.

---

## Collector Overview

| Collector | Source | Interval | Metrics | File |
|-----------|--------|----------|---------|------|
| **PDH** | Windows Performance Data Helper | 1 second | 8 | `src/windows/pdh_collector.cpp` |
| **Battery** | `GetSystemPowerStatus` Win32 API | 1 second | 5 | `src/windows/battery_collector.cpp` |
| **Thermal** | WMI ACPI (`MSAcpi_ThermalZoneTemperature`) | 5 seconds | 1 | `src/windows/thermal_collector.cpp` |
| **Process** | PDH process enumeration | 5 seconds | 30 (10×3) | `src/windows/process_collector.cpp` |

All collectors follow the same interface pattern:

```cpp
void register_<name>_collectors(MetricRegistry& registry);  // Called once at startup
void collect_<name>(MetricRegistry& registry);               // Called on each timer tick
```

---

## PDH Collector

The PDH (Performance Data Helper) collector provides core system metrics using Windows performance counters. It opens a single persistent query at startup and collects all counters atomically on each tick.

### Initialization

```
PdhOpenQuery → PdhAddEnglishCounter (×7) → PdhCollectQueryData (prime)
```

The initial `PdhCollectQueryData` call primes rate-based counters (CPU, disk I/O, network I/O), which require two data points to compute a rate. The first actual reading occurs on the second tick.

### PDH Counter Paths

| Metric Key | PDH Counter Path | Unit |
|-----------|-----------------|------|
| `cpu.total_pct` | `\Processor(_Total)\% Processor Time` | % |
| `mem.available_mb` | `\Memory\Available MBytes` | MB |
| `disk.active_pct` | `\PhysicalDisk(_Total)\% Disk Time` | % |
| `disk.read_bps` | `\PhysicalDisk(_Total)\Disk Read Bytes/sec` | B/s |
| `disk.write_bps` | `\PhysicalDisk(_Total)\Disk Write Bytes/sec` | B/s |
| `net.recv_bps` | `\Network Interface(*)\Bytes Received/sec` | B/s |
| `net.send_bps` | `\Network Interface(*)\Bytes Sent/sec` | B/s |

### Computed Metrics

| Metric Key | Source | Unit | Notes |
|-----------|--------|------|-------|
| `mem.used_pct` | `GlobalMemoryStatusEx` → `dwMemoryLoad` | % | Not from PDH — uses Win32 API directly |

### Data Quality

All PDH metrics report `Quality::Measured`. If `PdhCollectQueryData` or `PdhGetFormattedCounterValue` fails for a specific counter, that metric is silently skipped for that tick.

### Error Handling

- If `PdhOpenQuery` fails at startup, all PDH metrics are disabled
- If `PdhAddEnglishCounter` fails for a specific counter, a warning is logged and that counter is null for all future collections
- Failed individual counter reads are silently skipped (debug-level log)

---

## Battery Collector

The battery collector uses the `GetSystemPowerStatus` Win32 API to read AC status, battery level, charging state, and estimated time remaining.

### Metrics

| Metric Key | Display Name | Unit | Quality | Notes |
|-----------|-------------|------|---------|-------|
| `battery.level_pct` | Battery Level | % | Measured | 0-100. Omitted if `BatteryLifePercent == 255` (unknown) |
| `battery.charging` | Charging | bool | Measured | 1.0 if charging, 0.0 if not |
| `battery.ac_online` | AC Power | bool | Measured | 1.0 if AC connected, 0.0 if on battery |
| `power.current_w` | Power Draw | W | Unknown | Placeholder — actual wattage requires IOCTL_BATTERY_STATUS |
| `battery.remaining_min` | Time Remaining | min | Derived | Only available when on battery and OS provides estimate |

### Desktop Systems

On desktop PCs without a battery:
- `GetSystemPowerStatus` still succeeds
- `ACLineStatus` = 1 (always on AC)
- `BatteryLifePercent` = 255 (unknown) — `battery.level_pct` is not emitted
- `BatteryFlag` = 128 (no system battery)
- `BatteryLifeTime` = -1 — `battery.remaining_min` is not emitted

### Power Draw Estimation

The current implementation reports `power.current_w = 0.0` with `Quality::Unknown` because `GetSystemPowerStatus` does not provide wattage. A future implementation will use:

```
IOCTL_BATTERY_STATUS → BatteryRate (in mW, negative = discharging)
```

This requires opening the battery device via `SetupDiGetClassDevs` + `CreateFile` + `DeviceIoControl`.

---

## Thermal Collector

The thermal collector reads CPU/system thermal zone temperatures via WMI (Windows Management Instrumentation) and the ACPI thermal zone interface.

### Initialization

```
CoInitializeEx (COM) → CoCreateInstance (WbemLocator) →
ConnectServer (ROOT\WMI) → Test query (MSAcpi_ThermalZoneTemperature)
```

If WMI initialization fails or the test query returns no results, thermal monitoring is silently disabled for the session.

### Metrics

| Metric Key | Display Name | Unit | Quality | Source |
|-----------|-------------|------|---------|--------|
| `temp.zone_c` | Thermal Zone | °C | Measured | `MSAcpi_ThermalZoneTemperature.CurrentTemperature` |

### Temperature Conversion

WMI returns temperatures in **tenths of Kelvin**. Conversion:

```
celsius = (CurrentTemperature / 10.0) - 273.15
```

### Availability

Thermal zone data is available on most laptops and some desktops with ACPI-compliant motherboards. It is NOT available on:
- Many desktop PCs (no ACPI thermal zones exposed)
- Virtual machines
- Systems where WMI `ROOT\WMI` access is restricted

If unavailable, a log message notes "Thermal zone data not available on this hardware" at startup. No error is raised.

### Performance

WMI queries take approximately **50-200ms** per call. This is why the thermal collector uses a 5-second interval instead of 1 second.

---

## Process Collector

The process collector identifies the top 10 processes by CPU usage. It uses PDH wildcard counters to enumerate all running processes.

### Metrics

For each of the top 10 processes (i = 0..9):

| Metric Key | Display Name | Unit | Quality |
|-----------|-------------|------|---------|
| `proc.top<i>.cpu_pct` | Top Process #N CPU | % | Measured |
| `proc.top<i>.mem_mb` | Top Process #N Memory | MB | Measured |
| `proc.top<i>.name` | Top Process #N Name | text | Measured |

If fewer than 10 processes are running, remaining slots report `cpu_pct = 0.0` with `Quality::Unknown`.

### Collection Algorithm

1. Open a temporary PDH query with wildcard counter: `\Process(*)\% Processor Time`
2. Call `PdhCollectQueryData` twice with a 100ms pause (rate counters need two data points)
3. Call `PdhGetFormattedCounterArrayW` to get all process CPU values
4. Filter out `_Total` and `Idle` pseudo-processes
5. Sort by CPU descending
6. Take top 10

### Process Name Encoding

The `proc.top<i>.name` metric stores a hash of the process name (via `std::hash<std::string>`) as a double value. This is a temporary implementation detail — a future version will store names in a separate string table or include them in the JSON API response directly.

### Performance

The process collector is the most expensive collector due to:
- Opening and closing a temporary PDH query each run
- 100ms deliberate sleep for rate counter priming
- Enumerating all running processes

This is why it runs at a 5-second interval. On systems with many processes (500+), expect collection to take 200-500ms.

### Filtered Processes

The following pseudo-processes are automatically excluded:
- `_Total` — Aggregate of all processes
- `Idle` — System idle process

---

## Adding a Custom Collector

To add a new collector module:

1. Create `src/windows/your_collector.cpp`
2. Register metric keys and implement collection function
3. Declare in `include/pulseport/collectors.h`
4. Add to the sampler in `apps/pulseport-service/main.cpp`
5. Add the source file to `src/CMakeLists.txt` under `pulseport-windows`

### Metric Key Naming Convention

```
<category>.<metric_name>
```

| Category | Examples |
|----------|---------|
| `cpu` | `cpu.total_pct`, `cpu.core0_pct` |
| `mem` | `mem.available_mb`, `mem.used_pct` |
| `disk` | `disk.active_pct`, `disk.read_bps` |
| `net` | `net.recv_bps`, `net.send_bps` |
| `battery` | `battery.level_pct`, `battery.charging` |
| `power` | `power.current_w` |
| `temp` | `temp.zone_c`, `temp.cpu_c` |
| `proc` | `proc.top0.cpu_pct` |
| `gpu` | `gpu.usage_pct` (future) |

### Quality Enum

Always report the most accurate quality level:

| Quality | When to Use |
|---------|-------------|
| `Measured` | Direct hardware/OS reading (PDH, Win32 API) |
| `Derived` | Computed from other measured values (e.g., Wh from power × time) |
| `Estimated` | Heuristic or statistical approximation |
| `Unknown` | Sensor not available, placeholder value |

---

## Full Metric Registry

Complete list of all metrics registered by the default collectors:

| # | Key | Name | Unit | Category | Source | Interval |
|---|-----|------|------|----------|--------|----------|
| 1 | `cpu.total_pct` | CPU Total | % | cpu | pdh | 1s |
| 2 | `mem.available_mb` | Memory Available | MB | memory | pdh | 1s |
| 3 | `mem.used_pct` | Memory Used | % | memory | computed | 1s |
| 4 | `disk.active_pct` | Disk Active Time | % | disk | pdh | 1s |
| 5 | `disk.read_bps` | Disk Read | B/s | disk | pdh | 1s |
| 6 | `disk.write_bps` | Disk Write | B/s | disk | pdh | 1s |
| 7 | `net.recv_bps` | Network Receive | B/s | network | pdh | 1s |
| 8 | `net.send_bps` | Network Send | B/s | network | pdh | 1s |
| 9 | `battery.level_pct` | Battery Level | % | battery | system | 1s |
| 10 | `battery.charging` | Charging | bool | battery | system | 1s |
| 11 | `battery.ac_online` | AC Power | bool | battery | system | 1s |
| 12 | `power.current_w` | Power Draw | W | power | battery_rate | 1s |
| 13 | `battery.remaining_min` | Time Remaining | min | battery | system | 1s |
| 14 | `temp.zone_c` | Thermal Zone | °C | temperature | wmi_acpi | 5s |
| 15-44 | `proc.top[0-9].*` | Top Process #N | various | process | pdh_proc | 5s |
