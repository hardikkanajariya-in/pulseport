# Architecture

## Overview

PulsePort is a single-process C++20 Windows application that collects system metrics, stores historical data in an embedded SQLite database, and serves a real-time web dashboard.

## Thread Model

The application runs on **5 dedicated threads** with clear ownership boundaries:

| Thread | Responsibility | Blocking? |
|--------|---------------|-----------|
| **Main** | Initialization, shutdown coordination | Yes (waits for stop signal) |
| **Sampler** | WaitableTimer-driven metric collection at 1s intervals | Yes (WaitForSingleObject) |
| **Aggregator** | 1-minute and 15-minute rollup flushes to SQLite | Yes (sleep between flushes) |
| **HTTP/WS** | cpp-httplib server serving API, WebSocket, and static files | Yes (listen) |
| **Maintenance** | Retention pruning, WAL checkpoint (runs every hour) | Yes (sleep) |

### Concurrency Rules

1. **Ring buffers** are lock-free SPSC (single producer, single consumer) — the sampler writes, the HTTP thread reads snapshots.
2. **SQLite** uses WAL mode with a single-writer pattern — only the aggregator and maintenance threads write.
3. **MetricRegistry** uses a mutex for registration (rare), but snapshot reads are lock-free from ring buffers.

## Data Flow

```
Collectors (PDH, WMI, Win32)
     │
     ▼
 Sampler (1s tick)
     │
     ├──▶ Ring Buffer (per metric, 2048 slots)
     │         │
     │         ├──▶ WebSocket delta push (1s)
     │         └──▶ HTTP /live/snapshot (on demand)
     │
     └──▶ Aggregator
              │
              ├──▶ metric_1m (every 60s)
              ├──▶ metric_15m (every 900s, from 1m data)
              └──▶ energy_daily (Wh accumulation)
```

## Metric Collection

### PDH Counters (1s interval)
- `cpu.total_pct` — Total CPU utilization
- `mem.used_pct`, `mem.available_mb` — Memory status
- `disk.active_pct`, `disk.read_bps`, `disk.write_bps` — Disk I/O
- `net.recv_bps`, `net.send_bps` — Network throughput

### Battery & Power (1s interval)
- `battery.level_pct`, `battery.ac_online`, `battery.charging`
- `battery.remaining_min` — Estimated time remaining
- `power.current_w` — Current power draw (requires IOCTL_BATTERY_STATUS)

### Thermal (5s interval)
- `thermal.zone_N_c` — Temperature in Celsius (WMI MSAcpi_ThermalZoneTemperature)

### Process (5s interval)
- Top-10 processes by CPU utilization (live data only, not stored)

## Storage Schema

SQLite database at `%ProgramData%\PulsePort\pulseport.db` (or next to exe in console mode).

- **metric_current** — Latest value per metric (upserted each sample)
- **metric_1m** — 1-minute aggregates (min/avg/max/count, INTEGER bucket_ts)
- **metric_15m** — 15-minute aggregates (rolled up from 1m)
- **energy_daily** — Daily Wh totals and average power
- **events** — System lifecycle events (start, stop, suspend, resume)
- **metric_registry** — Known metric metadata (name, unit, quality)

## Security Model

- **Loopback-only binding** — `127.0.0.1` by default, no remote access
- **CSRF protection** — Origin header validation on POST requests
- **No authentication** — Not needed for localhost-only service
- **Request size limit** — 1 MB payload cap on POST bodies
- **Audit trail** — All deletions logged to `deletions_audit` table

## Build System

- **CMake 3.25+** with vcpkg manifest mode
- **MSVC** (Visual Studio 2022, v143 toolset)
- Dependencies managed via `vcpkg.json`
- Frontend built with Vite, output embedded in `web/dist/`
- MSI installer built with WiX v4

## Directory Structure

```
pulseport/
├── apps/pulseport-service/    # Entry point (main.cpp)
├── include/pulseport/         # Public headers
├── src/
│   ├── core/                  # MetricRegistry, Sampler
│   ├── storage/               # Database, MigrationRunner, Writer, Reader
│   ├── server/                # HTTP server + API routes
│   ├── config/                # Configuration management
│   ├── diagnostics/           # Self-metrics
│   ├── metrics/               # Aggregator, PowerPipeline
│   └── windows/               # Win32 collectors + service control
├── web/                       # Vite + Preact frontend
├── db/migrations/             # SQL migration files
├── packaging/windows/         # WiX installer definition
├── tests/                     # Unit + integration + perf tests
└── .github/workflows/         # CI + release pipelines
```
