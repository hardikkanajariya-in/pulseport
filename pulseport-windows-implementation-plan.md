# PulsePort Windows v1 Implementation Plan

## Overview

PulsePort is a Windows-first, open-source local observability service for laptops and desktops. It runs as a lightweight background process, starts automatically with Windows, exposes a localhost web dashboard, streams real-time telemetry over WebSocket, and stores one year of history in a local SQLite database.[cite:111][cite:96][cite:65]

The first release targets Windows only. Linux support is intentionally deferred so the architecture can stabilize around one collector model, one storage model, and one transport model before the cross-platform layer is introduced.[cite:31][cite:45][cite:111]

The product goal is to answer five questions well:

- What is the machine doing **right now**?
- How many watts is it drawing right now?
- How much energy was consumed today?
- What changed over time?
- What evidence is available for later analysis?

## Product goals

### Primary goals

- Real-time system telemetry in a browser on `127.0.0.1`.[cite:65][cite:75]
- Real-time power draw in watts when Windows exposes the underlying battery and system telemetry needed to derive it.[cite:31][cite:45]
- Daily energy accounting stored as Wh, with rollups and trend history up to 1 year.[cite:96]
- Low-overhead background runtime suitable for always-on execution.[cite:111][cite:124]
- User-controlled deletion of recorded history.[cite:96]
- Clean separation between native collection and web presentation.[cite:65][cite:75]

### Non-goals for v1

- Remote internet-facing access by default.
- Multi-device fleet monitoring.
- Linux support in the first release.
- GPU overclocking, fan control, or hardware tuning.
- Driver-level hardware access beyond supported Windows telemetry interfaces.[cite:31][cite:45]

## Operating principles

### Principle 1: Windows-native collection, browser-native UI

PulsePort should not build a large native desktop UI in C++. The C++ process should collect and normalize telemetry, then serve a web app and a live stream locally.[cite:65][cite:75]

### Principle 2: Event-driven server, coordinated sampling

The system should not create multiple busy loops or per-widget timers. The service should use one coordinated sampling scheduler, one aggregation pipeline, and an asynchronous local server based on a proven HTTP/WebSocket stack.[cite:65][cite:71][cite:124]

### Principle 3: Tiered retention, not raw forever

A one-year history is practical only if PulsePort stores rollups rather than continuous raw high-frequency samples in SQLite. SQLite WAL mode is well-suited for concurrent read-heavy applications with a single controlled writer pattern.[cite:96][cite:97][cite:122]

### Principle 4: Local-first security

The service should bind to loopback only by default. Browser access should be local, with optional auth added later if remote access is ever introduced.[cite:65][cite:75]

## User-facing feature set

### Live dashboard

The main dashboard should provide:

- CPU total utilization.
- Per-core CPU utilization.
- Memory used, available, committed, and pressure indicators.
- Disk active time, read throughput, and write throughput.
- Network receive and transmit throughput.
- Battery status, current charge level, charging/discharging state, estimated live power draw, and health indicators where available.[cite:31][cite:45]
- Machine temperature, thermal zone, or sensor-backed readings where available through supported Windows telemetry paths.[cite:31][cite:45]
- Process list for top contributors to CPU, memory, disk, and network activity, where available through Windows performance interfaces.[cite:31]

### Power and energy tracking

PulsePort should expose two different concepts clearly:

- **Power**: instantaneous or near-real-time draw, represented in watts.
- **Energy**: accumulated use over time, represented in Wh or kWh.

The dashboard should show:

- Current watt draw.
- Average watt draw over 1 minute, 5 minutes, and 15 minutes.
- Energy consumed today.
- Energy consumed yesterday.
- Energy consumed by day across the last 30 days.
- Energy trends across week, month, quarter, and year.
- Battery discharge versus charging periods.
- Gaps or unknown values when Windows cannot provide the required signal reliably.

### History and analysis

The history experience should include:

- Last 15 to 30 minutes at high resolution.
- Last 24 hours at minute resolution.
- Last 90 days at minute or 15-minute resolution, depending on cost targets.
- Last 365 days at 15-minute and daily resolution.
- Daily activity summaries with min, max, average, uptime window, and anomaly markers.
- Search and filter by date range.
- Export to CSV for selected time ranges.
- Manual delete for a day, range, or all history.

### Alerts and event log

The service should record notable events such as:

- High battery drain.
- Unexpected battery level drops.
- High CPU sustained for N minutes.
- High memory sustained for N minutes.
- Abnormal disk or network bursts.
- Service restart.
- Sleep/resume.
- User deletion events.
- Collector degradation, such as sensor unavailable or metric temporarily unknown.

## Technical architecture

## High-level design

PulsePort v1 should use a single-process service with clearly isolated modules inside the process boundary. A single process minimizes deployment complexity, memory duplication, and inter-process coordination costs for a localhost-only product.[cite:111][cite:124]

Recommended internal modules:

1. **Host service layer** — Windows service wrapper, lifecycle, shutdown, restart policy integration.[cite:111]
2. **Collector layer** — reads Windows telemetry sources and normalizes them.
3. **Metric engine** — computes derived values such as rates, rolling averages, and Wh totals.
4. **Retention engine** — converts live samples into 1-minute, 15-minute, and daily aggregates.
5. **Storage layer** — manages SQLite schema, writes, reads, migrations, and retention cleanup.[cite:96][cite:97]
6. **Stream layer** — pushes changes to connected browser clients over WebSocket.[cite:65][cite:71]
7. **HTTP API layer** — serves REST-like endpoints for history, config, and delete actions.[cite:75]
8. **Static asset layer** — serves the frontend build files.
9. **Configuration layer** — persists port, retention limits, thresholds, and feature flags.
10. **Diagnostics layer** — internal self-metrics, logs, and crash-safe shutdown handling.

### Recommended repository layout

```text
pulseport/
├─ apps/
│  ├─ pulseport-service/
│  └─ pulseport-web/
├─ src/
│  ├─ core/
│  ├─ windows/
│  ├─ metrics/
│  ├─ storage/
│  ├─ server/
│  ├─ config/
│  └─ diagnostics/
├─ include/
├─ db/
│  ├─ migrations/
│  └─ queries/
├─ web/
│  ├─ src/
│  └─ dist/
├─ packaging/
│  ├─ windows/
│  └─ scripts/
├─ tests/
│  ├─ unit/
│  ├─ integration/
│  └─ perf/
├─ docs/
└─ third_party/
```

### Technology choices

| Concern | Recommendation | Why |
|---|---|---|
| Language | C++20 | Strong Windows-native access with modern language features |
| HTTP | `cpp-httplib` for v1, Beast as scale-up path | Fast to integrate locally; Beast remains the advanced upgrade path.[cite:75][cite:65] |
| Live stream | WebSocket | Efficient push updates for dashboard refresh.[cite:65][cite:71] |
| Database | SQLite | Embedded, reliable, local-first storage.[cite:96] |
| SQLite mode | WAL | Better fit for one writer and concurrent readers.[cite:96][cite:97] |
| Frontend | React/Vite or similar static bundle | Easy charts, routing, theming, and maintainable UI |
| Charts | Browser-side charting library | Keeps native process focused on telemetry collection |
| Packaging | MSI or installer + Windows service registration | Clean Windows deployment |

## Windows telemetry strategy

### Approved telemetry paths

The collector should stay inside supported Windows interfaces wherever possible:

- PDH and Performance Counters for CPU, memory, disk, and network telemetry.[cite:31]
- ETW or TraceLogging-backed integration for more advanced event tracing or future extensions.[cite:45]
- Battery and system status interfaces for charge/discharge, capacity, and state exposure where available through Windows-supported layers.[cite:31][cite:45]

This keeps the design maintainable and avoids unstable direct-hardware assumptions that vary by vendor, firmware, and driver stack.[cite:31][cite:45]

### Power measurement strategy

The most important architectural rule is to separate **measured power**, **derived power**, and **estimated power**.

1. **Measured power**: a watts value exposed by Windows or hardware-backed telemetry that can be read directly.
2. **Derived power**: watts computed from battery energy change over time.
3. **Estimated power**: a fallback approximation inferred from load patterns when direct measurement is not available.

The UI should label the source quality explicitly. A professional tool should never present a guessed value as if it were a direct hardware reading.

### Recommended power pipeline

- First, attempt to read a direct battery discharge or charge rate if Windows exposes it through the available battery telemetry path.
- If a direct power-rate signal exists, normalize to watts.
- If only energy or capacity deltas are available, derive watts from change over time in a bounded rolling window.
- If the device is on AC and no power-rate signal is available, display `Unknown` rather than inventing a false precise number.
- Store a `quality` field with values such as `measured`, `derived`, `estimated`, or `unknown`.

### Coordinated sampling model

Even with an event-driven service, many system metrics still require regular sampling because the OS exposes counters or snapshots instead of push events. The correct design is a coordinated scheduler with bounded sample frequencies.

Recommended base schedule:

| Metric class | Frequency | Notes |
|---|---|---|
| Live counters for UI | 1 second | CPU, memory, disk, network, battery, power |
| Rolling derived metrics | 1 second | Update in-memory rolling windows |
| 1-minute aggregation | every 60 seconds | Flush aggregate rows to SQLite |
| 15-minute aggregation | every 15 minutes | Build longer-term history rows |
| Daily summary | at day rollover and on shutdown | Finalize daily totals |
| Maintenance | low-frequency | WAL checkpoint, retention cleanup, vacuum planning |

This is not a “looped tracking mechanism” in the wasteful sense. It is a bounded coordinated scheduler with one timer wheel or one async timer source driving all sample tasks, which is the normal systems approach for low-overhead telemetry software.[cite:124]

## Data retention model

### Why tiered storage is mandatory

One year of raw per-second samples across many metrics would create unnecessary database growth and write amplification. SQLite performs best in local embedded scenarios when writes are controlled and structured, not when every metric change is flushed immediately.[cite:96][cite:97]

### Recommended retention tiers

| Tier | Resolution | Retention | Storage location | Purpose |
|---|---|---|---|---|
| Live buffer | 1 second | 30 minutes | RAM ring buffer | Smooth live charts |
| Short history | 1 minute | 90 days | SQLite | Recent analysis |
| Long history | 15 minutes | 365 days | SQLite | Trend analysis |
| Daily summary | 1 day | 365 days or more | SQLite | Reports and dashboards |
| Event log | event-based | 365 days | SQLite | Audit and anomalies |

### Ring buffer approach

Use fixed-capacity per-metric ring buffers in memory for the live dashboard. The browser requests the latest snapshot and then receives delta updates over WebSocket. This design avoids constant database reads for active dashboards and keeps the UI smooth under load.[cite:65][cite:71]

## SQLite design

### Database configuration

Recommended startup pragmas:

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;
PRAGMA temp_store = MEMORY;
PRAGMA busy_timeout = 5000;
```

WAL mode is the key default because SQLite documents WAL as a persistent journaling mode that improves concurrency between readers and writers compared with rollback journaling.[cite:96][cite:97]

### Schema outline

```sql
CREATE TABLE app_config (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL,
  updated_at_utc TEXT NOT NULL
);

CREATE TABLE devices (
  id TEXT PRIMARY KEY,
  hostname TEXT NOT NULL,
  os_name TEXT NOT NULL,
  os_version TEXT NOT NULL,
  cpu_model TEXT,
  memory_bytes INTEGER,
  created_at_utc TEXT NOT NULL,
  updated_at_utc TEXT NOT NULL
);

CREATE TABLE metric_current (
  metric_key TEXT PRIMARY KEY,
  ts_utc TEXT NOT NULL,
  value_real REAL,
  unit TEXT NOT NULL,
  quality TEXT NOT NULL,
  source TEXT NOT NULL
);

CREATE TABLE metric_1m (
  bucket_start_utc TEXT NOT NULL,
  metric_key TEXT NOT NULL,
  min_value REAL,
  max_value REAL,
  avg_value REAL,
  sample_count INTEGER NOT NULL,
  quality TEXT NOT NULL,
  PRIMARY KEY (bucket_start_utc, metric_key)
);

CREATE TABLE metric_15m (
  bucket_start_utc TEXT NOT NULL,
  metric_key TEXT NOT NULL,
  min_value REAL,
  max_value REAL,
  avg_value REAL,
  sample_count INTEGER NOT NULL,
  quality TEXT NOT NULL,
  PRIMARY KEY (bucket_start_utc, metric_key)
);

CREATE TABLE energy_daily (
  day_local TEXT PRIMARY KEY,
  energy_wh REAL,
  avg_power_w REAL,
  peak_power_w REAL,
  charge_energy_wh REAL,
  discharge_energy_wh REAL,
  active_seconds INTEGER NOT NULL,
  quality TEXT NOT NULL,
  finalized INTEGER NOT NULL DEFAULT 0,
  updated_at_utc TEXT NOT NULL
);

CREATE TABLE events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  ts_utc TEXT NOT NULL,
  severity TEXT NOT NULL,
  category TEXT NOT NULL,
  title TEXT NOT NULL,
  payload_json TEXT
);

CREATE TABLE deletions_audit (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  ts_utc TEXT NOT NULL,
  scope TEXT NOT NULL,
  range_start TEXT,
  range_end TEXT,
  note TEXT
);
```

### Indexes

```sql
CREATE INDEX idx_metric_1m_key_time ON metric_1m(metric_key, bucket_start_utc);
CREATE INDEX idx_metric_15m_key_time ON metric_15m(metric_key, bucket_start_utc);
CREATE INDEX idx_events_time ON events(ts_utc);
CREATE INDEX idx_events_category_time ON events(category, ts_utc);
```

### Write strategy

- Exactly one writer queue for all persistent inserts and updates.
- Short transactions, batched by time bucket.
- Separate read connections for API requests.
- Avoid synchronous writes for every WebSocket update.
- Flush live data to SQLite only on bucket boundaries or notable events.
- Run checkpoint policy deliberately rather than constantly.

### Delete strategy

User deletion is a product feature, not a raw SQL side effect. Support these actions:

- Delete one day.
- Delete a custom date range.
- Delete all history while preserving configuration.
- Optionally delete only event logs or only metric history.

Every delete action should:

1. Validate the requested range.
2. Log an audit row in `deletions_audit`.
3. Remove matching rows transactionally.
4. Recompute impacted summaries if partial ranges were removed.
5. Notify connected dashboards so the UI refreshes immediately.

## Service lifecycle

### Windows service behavior

PulsePort should ship as a real Windows service rather than a tray app for v1. Microsoft documents automatically starting services through the Windows Service Control Manager, which is the correct model for an always-on background collector.[cite:111]

Recommended service properties:

- Startup type: Automatic (Delayed Start is worth evaluating on lower-end devices).
- Service account: LocalService unless a specific requirement needs LocalSystem.
- Recovery: restart on first and second failure.
- Graceful stop timeout with flush of pending aggregates.
- Event log registration for startup, failure, and shutdown.

### Boot and resume behavior

The service should:

- Start automatically on boot.[cite:111]
- Detect resume from sleep or hibernate.
- Mark a timeline gap if telemetry continuity is broken.
- Finalize the previous open bucket when the clock crosses a day boundary.
- Rebuild in-memory caches from recent SQLite buckets on service start.

## API design

### HTTP endpoints

Recommended localhost API surface:

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/health` | Service health and version |
| GET | `/api/system/info` | Host metadata and supported capabilities |
| GET | `/api/live/snapshot` | Current normalized metric snapshot |
| GET | `/api/history` | Time-series query by metric and range |
| GET | `/api/energy/daily` | Daily power-consumption history |
| GET | `/api/events` | Event log query |
| POST | `/api/config` | Update settings |
| POST | `/api/history/delete` | Delete selected history |
| POST | `/api/alerts/test` | Test alerts pipeline |

### WebSocket channels

Recommended channels or message types:

- `snapshot` — complete current values on initial connect.
- `delta` — changed metrics only.
- `event` — alerts, restarts, deletions, sensor loss.
- `status` — collector status, backfill, and maintenance state.

### Message contract example

```json
{
  "type": "delta",
  "tsUtc": "2026-04-03T05:40:01Z",
  "metrics": [
    {
      "key": "power.current_w",
      "value": 21.7,
      "unit": "W",
      "quality": "derived",
      "source": "battery_rate"
    },
    {
      "key": "cpu.total_pct",
      "value": 14.2,
      "unit": "%",
      "quality": "measured",
      "source": "pdh"
    }
  ]
}
```

## Frontend plan

### UI architecture

The web UI should be static files built once and served by the C++ service. This avoids shipping a separate Node runtime in production while preserving modern frontend developer experience.

Recommended frontend sections:

- Overview dashboard.
- Power dashboard.
- History explorer.
- Event log.
- Settings.
- Data management.
- Diagnostics.

### UX requirements

- Browser connects instantly to localhost.
- The page renders a cached shell even if live data is still initializing.
- Every metric tile shows value, unit, timestamp, and data-quality label.
- Historical charts clearly distinguish live, aggregated, and unknown periods.
- Delete actions require confirmation and show irreversible scope.
- The UI should visually separate power from energy so users do not confuse W with Wh.

### Suggested charts

- Live power line chart, 30-minute window.
- Daily energy bar chart, 30-day window.
- Week-over-week energy comparison.
- CPU, memory, disk, network sparklines.
- Event overlay markers on time-series charts.
- Heatmap of hour-of-day versus power draw for selected date ranges.

## Performance budget

### Resource targets

For an always-on service, define explicit budgets:

| Resource | Target |
|---|---|
| Idle CPU | below 1% on a typical modern machine |
| Memory | ideally 30-80 MB service-side excluding browser tab |
| Disk writes | batched and bounded; no per-sample fsync pattern |
| Startup | service ready within a few seconds after SCM start |
| Live latency | under 1 second end-to-end for normal updates |

### Optimization rules

- One sampling coordinator, not many threads waking independently.
- Fixed-size ring buffers for live metrics.
- Preallocated objects for hot paths where practical.
- Minimize JSON allocations in the WebSocket path.
- Avoid querying SQLite for every dashboard repaint.
- Use change-threshold streaming so tiny meaningless deltas do not flood clients.
- Run expensive maintenance only on sparse schedules.

## Reliability and correctness

### Correctness rules

- All timestamps stored in UTC.
- Daily summaries computed using a configured local timezone boundary.
- Power and energy never mixed in the same unit field.
- Missing values stored explicitly as unknown, not silently zeroed.
- Resume and shutdown flushes must finalize open buckets safely.
- Schema migrations must be versioned and reversible where feasible.

### Crash safety

SQLite WAL provides resilience advantages for local crash recovery scenarios, but the application still needs safe write boundaries and restart reconstruction logic.[cite:96][cite:97]

On restart, PulsePort should:

1. Open the database.
2. Validate schema version.
3. Reconstruct recent in-memory ring buffers from the latest persisted buckets.
4. Mark a restart event.
5. Resume coordinated sampling.

## Security model

### Local security baseline

- Bind to `127.0.0.1` only by default.
- No external listen address in v1.
- Use anti-CSRF and origin checks for state-changing POST endpoints.
- Validate delete ranges server-side.
- Cap request size for config and export endpoints.
- Sanitize all event payloads rendered in the UI.

### Future-ready security hooks

Even though v1 is localhost-only, reserve space in the config and auth model for:

- Optional token auth.
- Optional LAN mode.
- TLS termination when remote mode is introduced.
- Role-based delete/export permissions.

## Logging and diagnostics

### Internal logs

Maintain structured logs for:

- Service startup and shutdown.
- Collector attach/detach and capability detection.
- SQLite open, migrate, checkpoint, and failure events.[cite:96][cite:122]
- WebSocket connection counts.
- API errors.
- Retention cleanup operations.
- Deletion audit operations.

### Self-diagnostics page

The diagnostics page should show:

- App version and build hash.
- Uptime.
- Last sample time.
- Last aggregation flush time.
- Queue depth.
- Database size.
- WAL size.
- Connected browser sessions.
- Supported metric capabilities.
- Recent internal warnings.

## Testing strategy

### Unit tests

- Metric normalization.
- Power derivation math.
- Energy accumulation.
- Bucket rollup correctness.
- Deletion range validation.
- Timezone and day-boundary logic.
- JSON message serialization.

### Integration tests

- Service start and stop.
- Database migration and rollback scenarios.
- WebSocket stream handshake and delta delivery.[cite:65][cite:71]
- History query correctness.
- Delete and refresh workflow.
- Resume from simulated restart.

### Performance tests

- 24-hour soak test.
- Rapid browser connect/disconnect.
- High-frequency update storm with bounded CPU.
- Database growth validation over simulated 365-day retention.
- Cold start on a database with full one-year history.

### Real-machine validation

PulsePort should be validated on:

- A battery-backed Windows laptop.
- A desktop without battery telemetry.
- A lower-end Windows machine.
- A machine with sleep/resume cycles.
- A machine with intermittent sensor availability.

## Delivery phases

### Phase 0: foundation

- Repository setup.
- Build system.
- Service host skeleton.
- SQLite bootstrap with migrations.
- Static file serving.
- Basic health endpoint.

### Phase 1: live telemetry core

- CPU, memory, disk, network, battery collectors.
- Unified metric registry.
- Ring buffer implementation.
- WebSocket snapshot and delta stream.
- Overview dashboard.

### Phase 2: power and energy

- Direct or derived watts pipeline.
- Quality labeling.
- Daily energy accumulation.
- Power dashboard.
- Initial event log.

### Phase 3: persistence and history

- 1-minute and 15-minute rollups.
- Daily summaries.
- History explorer.
- CSV export.
- Delete workflows.

### Phase 4: hardening

- Performance tuning.
- Recovery policies.
- Installer and service registration.
- Soak tests.
- Diagnostics page.

### Phase 5: release

- Open-source docs.
- Configuration reference.
- Contribution guide.
- Sample screenshots and demo data.

## Open-source project standards

Because PulsePort is an open-source project, the repository should ship with:

- `README.md` with scope and quick start.
- `ARCHITECTURE.md` with module boundaries.
- `CONTRIBUTING.md` with coding rules and PR flow.
- `SECURITY.md` with vulnerability reporting.
- `LICENSE`.
- `CODE_OF_CONDUCT.md`.
- `docs/telemetry-model.md` for metric definitions.
- `docs/storage-model.md` for retention and schema.

## Key risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Windows power telemetry differs by machine | Power feature inconsistency | Quality labels, capability detection, and graceful `unknown` states |
| Too many writes to SQLite | Higher I/O and lag | Tiered rollups, batching, WAL, one writer queue.[cite:96][cite:97] |
| Always-on service becomes heavy | User distrust | Strict CPU/RAM budgets and perf regression tests |
| Dashboard reads overload storage | UI lag | Use RAM ring buffers and push deltas over WebSocket.[cite:65][cite:71] |
| Delete logic corrupts summaries | Data integrity issues | Transactional deletes and recompute strategy |
| Sleep/resume breaks daily totals | Incorrect reports | Explicit gap handling and day-finalization rules |

## Final recommendations

PulsePort v1 should be implemented as a Windows service with a local async HTTP/WebSocket server, a coordinated sampling engine, and SQLite-backed tiered history in WAL mode.[cite:111][cite:65][cite:96]

The most important engineering decision is not the dashboard framework but the telemetry model: treat real-time power, daily energy, historical rollups, and data quality as first-class concepts from the first commit. That decision will determine correctness, performance, and long-term cross-platform portability more than any UI choice.

The most practical next artifacts to write after this plan are:

1. `ARCHITECTURE.md`
2. `db/schema.sql`
3. `docs/api-contract.md`
4. `docs/metric-registry.md`
5. `docs/windows-collector-design.md`
6. `packaging/windows/service-install.ps1`
