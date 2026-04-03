# Troubleshooting Guide

Common issues, diagnostics steps, and solutions for PulsePort.

---

## Service Won't Start

### Symptom: Service stops immediately after start

**Check the log file:**
```powershell
Get-Content "C:\ProgramData\PulsePort\logs\pulseport.log" -Tail 50
```

**Common causes:**

| Cause | Log Message | Fix |
|-------|-------------|-----|
| Port already in use | `HTTP server failed to start on 127.0.0.1:9770` | Change port in `config.json` or stop the conflicting process: `Get-Process -Id (Get-NetTCPConnection -LocalPort 9770).OwningProcess` |
| Database locked | `Failed to open database` | Check if another process has a lock: `handle.exe pulseport.db` (Sysinternals). Restart the conflicting process. |
| Missing migrations dir | `No migrations found` | Ensure `db/migrations/` exists relative to the executable or set `migrations_dir` in config |
| Permission denied | `Access denied` on ProgramData path | Run service as LocalSystem or grant write access to `C:\ProgramData\PulsePort\` |
| Invalid config JSON | `Failed to parse config` | Validate JSON: `Get-Content config.json | ConvertFrom-Json` |

### Symptom: Service starts but dashboard shows nothing

1. Check the service is running: `Get-Service PulsePort`
2. Test the API: `Invoke-RestMethod http://127.0.0.1:9770/api/v1/health`
3. Check browser console for errors (F12 → Console)
4. Verify the web directory exists: `Test-Path "C:\Program Files\PulsePort\web"`

---

## No Metrics Showing

### All metrics missing

**Cause:** PDH initialization failed.

**Check:**
```powershell
# Verify PDH counters are available
Get-Counter "\Processor(_Total)\% Processor Time" -SampleInterval 1 -MaxSamples 1
```

If this fails, the Windows Performance Counter subsystem is broken. Fix:
```powershell
# Rebuild performance counters (Administrator)
lodctr /r
winmgmt /resyncperf
```

### Battery metrics show 0 or "unknown"

**Desktop PCs:** `power.current_w` reports `0.0` with `Quality::Unknown` on systems without a battery. This is expected.

**Laptops with missing battery data:**
1. Check if `GetSystemPowerStatus` works:
   ```powershell
   [System.Windows.Forms.SystemInformation]::PowerStatus
   ```
2. If `BatteryLifePercent` is -1 (255), the battery driver is not reporting data.
3. Update your laptop's battery/ACPI drivers.

### Thermal data missing

Thermal zone data requires ACPI-compliant hardware. Check availability:

```powershell
Get-WmiObject -Namespace "root\WMI" -Class MSAcpi_ThermalZoneTemperature 2>$null
```

If this returns nothing, thermal monitoring is not available on your hardware. This is common on:
- Desktop PCs without ACPI thermal zones
- Virtual machines
- Some older laptops

The service logs `"Thermal zone data not available on this hardware"` at startup — this is informational, not an error.

### Process collector not updating

The process collector has a 5-second interval (not 1 second). Wait at least 10 seconds for data to appear.

If process CPU values seem wrong:
- The first collection after startup always shows 0% (rate counters need two data points)
- Values may exceed 100% on multi-core systems (PDH reports per-core percentages summed)

---

## Dashboard Issues

### Dashboard loads but shows "Disconnected"

The frontend shows "Disconnected" when it can't reach the API.

**Steps:**
1. Verify the backend is running:
   ```powershell
   Invoke-RestMethod http://127.0.0.1:9770/api/v1/health
   ```
2. If using the Vite dev server, ensure the proxy is configured to forward to port 9770
3. Check if a firewall or antivirus is blocking localhost connections

### Charts are empty

Charts require at least 2 data points. Wait 2+ minutes for 1-minute aggregation data to appear on the History page.

For the Dashboard page, live data appears via polling (snapshot endpoint). Check:
```powershell
Invoke-RestMethod http://127.0.0.1:9770/api/v1/live/snapshot | Select-Object -ExpandProperty metrics
```

### Dashboard shows stale data

The frontend polls every 1 second. If data appears frozen:
1. Check the Diagnostics page → `lastSampleTime` should be increasing
2. If `lastSampleTime` is stale, the sampler thread may have crashed — restart the service
3. Hard-refresh the browser (`Ctrl+Shift+R`)

---

## Database Issues

### Database file is very large

Check current size:
```powershell
(Get-Item "C:\ProgramData\PulsePort\pulseport.db").Length / 1MB
```

**Reduce size:**
1. Lower retention settings in `config.json`:
   ```json
   {
     "retention_1m_days": 30,
     "retention_15m_days": 180
   }
   ```
2. Delete old data via API:
   ```powershell
   $body = @{ scope = "all" } | ConvertTo-Json
   Invoke-RestMethod -Method POST -Uri http://127.0.0.1:9770/api/v1/history/delete -Body $body -ContentType "application/json"
   ```
3. Compact the database (requires stopping the service):
   ```powershell
   Stop-Service PulsePort
   sqlite3 "C:\ProgramData\PulsePort\pulseport.db" "VACUUM;"
   Start-Service PulsePort
   ```

### Database is locked / corruption

If the database becomes locked (usually from an unclean shutdown):

```powershell
Stop-Service PulsePort

# Check integrity
sqlite3 "C:\ProgramData\PulsePort\pulseport.db" "PRAGMA integrity_check;"

# Force WAL checkpoint
sqlite3 "C:\ProgramData\PulsePort\pulseport.db" "PRAGMA wal_checkpoint(TRUNCATE);"

Start-Service PulsePort
```

If `integrity_check` fails:
1. Back up the corrupted file
2. Attempt recovery:
   ```powershell
   sqlite3 "C:\ProgramData\PulsePort\pulseport.db" ".recover" | sqlite3 "C:\ProgramData\PulsePort\pulseport_recovered.db"
   ```
3. If recovery fails, delete the database — PulsePort will create a fresh one on start

### WAL file is very large

The WAL file normally stays small (<10 MB). If it grows large:
```powershell
# Manual checkpoint
sqlite3 "C:\ProgramData\PulsePort\pulseport.db" "PRAGMA wal_checkpoint(TRUNCATE);"
```

If this doesn't help, there may be a long-running reader preventing checkpoint. Restart the service.

---

## Performance Issues

### High CPU usage

PulsePort should use <1% CPU in steady state. If higher:

1. Check the process collector interval — the 100ms sleep during process enumeration adds up if the interval is too low
2. Check log level — `trace` or `debug` generates heavy file I/O
3. Check if another process is reading the database intensively

**Quick fix:** Increase intervals in config:
```json
{
  "process_interval_ms": 10000,
  "thermal_interval_ms": 10000,
  "log_level": "warn"
}
```

### High disk I/O

Most disk I/O comes from SQLite writes (every 60 seconds for aggregation) and log file writes.

**Reduce I/O:**
1. Set `log_level` to `warn` or `error` to reduce log writes
2. Ensure WAL mode is active (default) — check with:
   ```sql
   PRAGMA journal_mode;  -- Should return "wal"
   ```
3. Windows Defender real-time scanning can cause I/O spikes — consider excluding `C:\ProgramData\PulsePort`

### High memory usage

PulsePort should use 20-50 MB of RAM. If higher:

1. Check the `mmap_size` pragma — 64 MB allows SQLite to memory-map the database file
2. Check the number of registered metrics — each metric consumes a small amount of memory in the MetricRegistry and Aggregator working buckets
3. The process collector allocates a temporary buffer for PDH enumeration — this is freed after each collection

---

## Build Issues

### vcpkg dependencies not found

```
CMake Error: Could not find package httplib
```

**Fix:** Ensure the vcpkg toolchain is specified:
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

If `VCPKG_ROOT` is not set:
```powershell
$env:VCPKG_ROOT = "C:\vcpkg"  # Adjust to your vcpkg path
```

### Frontend build fails

```
ERR_PNPM_NO_MATCHING_VERSION
```

**Fix:** Clear the cache and reinstall:
```powershell
cd web
Remove-Item -Recurse -Force node_modules
pnpm install
pnpm exec vite build
```

### C++ compilation errors

Ensure you have the correct VS 2022 workload installed. Required:
- MSVC v143 (C++20 support)
- Windows 10/11 SDK (for PDH, WMI, Win32 APIs)

---

## Log Reference

### Log Location

```
C:\ProgramData\PulsePort\logs\pulseport.log
```

Rotation: 5 MB max file size, 3 rotated files (`pulseport.log`, `pulseport.1.log`, `pulseport.2.log`).

### Log Levels

| Level | When to Use | Verbose |
|-------|-------------|---------|
| `trace` | Development only | Very high — logs every sample |
| `debug` | Debugging specific issues | High — logs per-collection details |
| `info` | Normal operation (default) | Moderate — startup, shutdown, periodic summaries |
| `warn` | Something unexpected but recoverable | Low |
| `error` | Something failed | Very low — only failures |

### Changing Log Level at Runtime

Currently requires a service restart:
```powershell
# Edit config.json
# Change "log_level": "debug"
Restart-Service PulsePort
```

### Log Format

```
[2026-04-03 12:00:00.123] [12345] [info] PulsePort 0.1.0 starting
```

Fields: `[timestamp] [thread_id] [level] message`

---

## Getting Help

1. Check the [GitHub Issues](https://github.com/hardikkanajariya-in/pulseport/issues) for known issues
2. Review the log file for error messages
3. Open a new issue with:
   - PulsePort version (`pulseport-service.exe --version`)
   - Windows version (e.g., Windows 11 23H2)
   - Log file excerpt (last 50 lines)
   - Steps to reproduce
