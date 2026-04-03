# Architecture

PulsePort is a single-process, zero-dependency Windows telemetry service that collects system metrics, persists them in a local SQLite database, and serves a real-time web dashboard. This document describes the system design, component responsibilities, data flow, and concurrency model.

---

## High-Level Overview

```
┌────────────────────────────────────────────────────────────┐
│                    PulsePort Service                       │
│                                                            │
│  ┌──────────┐   ┌──────────────┐   ┌───────────────────┐  │
│  │ Sampler  │──▶│MetricRegistry│──▶│   Aggregator      │  │
│  │ (Timer)  │   │  (in-memory) │   │ (1m/15m buckets)  │  │
│  └────┬─────┘   └──────────────┘   └────────┬──────────┘  │
│       │                                      │             │
│  ┌────┴─────┐                          ┌─────┴──────┐     │
│  │Collectors│                          │   Storage   │     │
│  │ PDH      │                          │  Writer     │     │
│  │ Battery  │                          │  (SQLite)   │     │
│  │ Thermal  │                          └─────┬──────┘     │
│  │ Process  │                                │             │
│  └──────────┘                                │             │
│                                        ┌─────┴──────┐     │
│  ┌──────────────────────────────┐      │  Database   │     │
│  │      HTTP Server             │      │  (WAL mode) │     │
│  │  ┌────────┐  ┌───────────┐  │      └─────┬──────┘     │
│  │  │REST API│  │ Static    │  │             │             │
│  │  │/api/v1 │  │ Frontend  │  │◀────────────┘             │
│  │  └────────┘  └───────────┘  │      Storage Reader       │
│  │  ┌────────────────────────┐ │                            │
│  │  │WebSocket (planned)     │ │                            │
│  └──┴────────────────────────┴─┘                            │
│                                                            │
│  127.0.0.1:9770                                            │
└────────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### 1. Entry Point (`apps/pulseport-service/main.cpp`)

The service binary supports two execution modes:

| Mode | Flag | Description |
|------|------|-------------|
| **Service** | (default) | Registers with Windows Service Control Manager (SCM) |
| **Console** | `--console` / `-c` | Runs in foreground with stdout logging |

Startup sequence:
1. Parse CLI arguments (`--console`, `--config <path>`, `--version`, `--help`)
2. Load configuration from JSON file (falls back to defaults)
3. Resolve runtime paths (`%ProgramData%\PulsePort\`, exe-relative)
4. Initialize spdlog with rotating file + console + Windows Event Log sinks
5. Open SQLite database and run pending migrations
6. Create storage writer/reader over the database handle
7. Initialize `MetricRegistry` and register all collectors
8. Persist metric metadata to `metric_registry` table
9. Build `Aggregator` and `Sampler` with collector callbacks
10. Start sampler thread (non-blocking)
11. Start HTTP server on a dedicated `std::jthread`
12. Wait for shutdown signal (Ctrl+C in console, SCM stop in service)
13. Graceful shutdown: stop HTTP → stop sampler → final aggregator flush → close DB

### 2. MetricRegistry (`include/pulseport/metric_registry.h`)

Thread-safe in-memory store for metric metadata and the latest sample values.

**Responsibilities:**
- Register metric definitions (`MetricInfo`: key, display name, unit, source, category)
- Accept live `MetricSample` pushes from collectors
- Provide atomic snapshots of all current values for the API and WebSocket

**Thread safety:** Protected by a `std::mutex`. Collectors push from the sampler thread; the HTTP server reads from request threads.

### 3. Sampler (`include/pulseport/sampler.h`)

Centralized sampling scheduler using a Windows Waitable Timer for precise 1-second ticks.

**Design:**
- Maintains a list of `Collector` entries, each with a name, interval (ms), and callback function
- On each 1-second tick, increments elapsed time for each collector and fires those whose interval has elapsed
- The aggregator flush is registered as a "collector" with a 60-second interval
- Single thread drives all collectors to avoid clock skew between data sources

**Registered collectors and their intervals:**

| Collector | Interval | Source |
|-----------|----------|--------|
| `pdh` | 1000 ms | PDH performance counters |
| `battery` | 1000 ms | `GetSystemPowerStatus` |
| `thermal` | 5000 ms | WMI ACPI thermal zones |
| `processes` | 5000 ms | PDH process enumeration |
| `aggregator_flush` | 60000 ms | Aggregator 1-minute flush |

### 4. Collectors (`src/windows/`)

Four collector modules, each following the pattern:

```cpp
void register_<name>_collectors(MetricRegistry& registry);  // Register metric keys
void collect_<name>(MetricRegistry& registry);               // Push current values
```

See [Collectors Reference](collectors.md) for detailed metric keys and data sources.

### 5. Aggregator (`src/metrics/aggregator.cpp`)

Computes time-bucketed rollups from raw 1-second samples.

**Pipeline:**
```
1s samples → accumulate() → WorkingBucket (in-memory)
                                   │
                              flush_1m() (every 60s)
                                   │
                           metric_1m table (SQLite)
                                   │
                              flush_15m() (every 15m)
                                   │
                           metric_15m table (SQLite)
```

**WorkingBucket** tracks per-metric min, max, running sum, sample count, and worst data quality within the current 1-minute window. On flush, it produces a `MetricAggregate` and resets.

**15-minute rollups** are computed by reading the last 15 rows from `metric_1m` and producing a weighted average (by sample count), global min/max, and worst quality.

### 6. PowerPipeline (`src/metrics/power_pipeline.cpp`)

Dedicated pipeline for energy tracking:

- Maintains a rolling window of up to 900 1-second power readings (15 minutes)
- Accumulates energy in Wh using trapezoidal integration: `Wh += watts × (Δt / 3600)`
- Clamps Δt to ≤10 seconds to ignore sleep/resume gaps
- Provides `avg_watts(window_seconds)` for the dashboard
- Daily accumulator resets at day boundaries

### 7. Storage Layer (`src/storage/`)

**StorageWriter** — Single-writer with `std::mutex`:
- `write_current()`: Upserts `metric_current` table (latest values)
- `write_1m()` / `write_15m()`: Batch insert aggregates in a transaction
- `write_energy_daily()`: Upsert daily energy record
- `write_event()`: Insert system events
- `write_metric_info()`: Upsert metric registry

All batch operations use `BEGIN/COMMIT TRANSACTION` with `sqlite3_bind_*` parameterized queries.

**StorageReader** — Read-only queries with SQL injection protection (table name whitelist):
- `query_history()`: Fetches aggregates from `metric_1m` or `metric_15m` by key + time range
- `query_energy_daily()`: Fetches daily energy records by date range
- `query_events()`: Fetches events with optional category filter
- `delete_history()` / `delete_all_history()`: Deletes with audit trail

### 8. Database (`src/storage/database.cpp`)

SQLite 3.40+ with the following pragmas:

| Pragma | Value | Purpose |
|--------|-------|---------|
| `journal_mode` | WAL | Concurrent reads during writes |
| `synchronous` | NORMAL | Balance durability vs. performance |
| `mmap_size` | 67108864 (64 MB) | Memory-mapped I/O |
| `cache_size` | -16384 (16 MB) | In-memory page cache |

Migrations are applied from `db/migrations/` on startup, tracked by `schema_version` table.

### 9. HTTP Server (`src/server/http_server.cpp`)

Built on **cpp-httplib**, bound to `127.0.0.1` only (localhost).

**Features:**
- 1 MB request body limit
- CSRF protection on mutating endpoints (Origin header check)
- Static file serving from the frontend dist directory
- JSON responses for all API endpoints

See [API Reference](api-reference.md) for endpoint documentation.

### 10. Frontend (`web/`)

Vite 6 + Preact 10 SPA with uPlot charting.

**Pages:** Dashboard, Power, History, Events, Settings, Diagnostics

**Data flow:**
- WebSocket connection to `/ws` for live metric deltas (planned)
- Polling fallback via `GET /api/v1/live/snapshot` at 1-second intervals
- Preact Signals (`@preact/signals`) for reactive state management
- uPlot for lightweight, GPU-accelerated time-series charts

---

## Data Flow

### Collection → Persistence

```
Collector Fn ──push_sample()──▶ MetricRegistry ──snapshot()──▶ Aggregator
                                      │                           │
                                      │                      accumulate()
                                      │                           │
                                      ▼                    flush_1m() (60s)
                                HTTP Server                       │
                            /api/v1/live/snapshot            StorageWriter
                                                                  │
                                                             ┌────┴────┐
                                                             │ SQLite  │
                                                             │ (WAL)   │
                                                             └─────────┘
```

### API Request Flow

```
Browser ──HTTP──▶ cpp-httplib ──▶ Route Handler
                                       │
                              ┌────────┴────────┐
                              │                 │
                        MetricRegistry    StorageReader
                        (live data)       (historical)
                              │                 │
                              ▼                 ▼
                           JSON Response ──▶ Browser
```

---

## Concurrency Model

| Thread | Owner | Responsibility |
|--------|-------|----------------|
| **Main** | `main()` | Config, init, waits for shutdown signal |
| **Sampler** | `Sampler::run()` | Drives all collectors on a WaitableTimer tick |
| **HTTP** | `std::jthread` in `main()` | Handles HTTP requests (cpp-httplib thread pool) |
| **cpp-httplib pool** | Internal | Per-request handler execution |

**Shared resources and synchronization:**

| Resource | Threads | Protection |
|----------|---------|------------|
| `MetricRegistry` | Sampler + HTTP | `std::mutex` |
| `StorageWriter` | Sampler (aggregator) + HTTP (config) | `std::mutex` |
| `StorageReader` | HTTP only | None (SQLite WAL supports concurrent reads) |
| `Aggregator::buckets_` | Sampler only | `std::mutex` (defensive) |
| `SelfMetrics` | All | Atomic fields where needed |

---

## Security Boundaries

- **Network binding:** `127.0.0.1` only — no remote access by default
- **CSRF protection:** Mutating endpoints (`POST`) check `Origin` header for `127.0.0.1` or `localhost`
- **SQL injection:** Table names are whitelisted; all values use parameterized queries
- **Request limits:** 1 MB max payload
- **No authentication:** Intentional for local-only tool — but requires localhost binding

---

## Directory Structure

```
pulseport/
├── apps/pulseport-service/    # Service entry point
│   ├── main.cpp
│   └── CMakeLists.txt
├── db/migrations/             # SQL migration files
├── docs/                      # Documentation (you are here)
├── include/pulseport/         # Public C++ headers
├── packaging/windows/         # WiX MSI installer definitions
├── src/                       # C++ implementation
│   ├── config/                # Configuration management
│   ├── core/                  # MetricRegistry, Sampler
│   ├── diagnostics/           # Self-metrics
│   ├── metrics/               # Aggregator, PowerPipeline
│   ├── server/                # HTTP server + API routes
│   ├── storage/               # Database, Writer, Reader
│   └── windows/               # OS-specific collectors
├── tests/                     # Unit + integration tests
│   ├── unit/
│   ├── integration/
│   └── perf/
└── web/                       # Vite + Preact frontend
    ├── src/
    │   ├── components/
    │   ├── hooks/
    │   ├── lib/
    │   ├── pages/
    │   └── styles/
    └── public/
```

---

## Technology Stack

| Layer | Technology | Version |
|-------|-----------|---------|
| Language | C++20 | MSVC (VS 2022) |
| Build | CMake | 3.25+ |
| Package Manager | vcpkg | Manifest mode |
| HTTP | cpp-httplib | 0.40+ |
| Database | SQLite | 3.40+ |
| Logging | spdlog | 1.x |
| JSON | nlohmann-json | 3.x |
| Frontend | Preact | 10.x |
| Bundler | Vite | 6.x |
| Charts | uPlot | 1.x |
| State | @preact/signals | 1.x |
| Installer | WiX | v4 |
| CI/CD | GitHub Actions | — |
| Frontend PM | pnpm | 10.x |
