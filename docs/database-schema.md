# Database Schema Reference

PulsePort uses SQLite 3.40+ in WAL (Write-Ahead Logging) mode. The schema is managed through numbered SQL migration files in `db/migrations/`.

---

## Database Location

| Context | Path |
|---------|------|
| Windows Service | `C:\ProgramData\PulsePort\pulseport.db` |
| Custom | Set via `db_path` in `config.json` |
| Tests | `:memory:` (in-memory database) |

The WAL file (`pulseport.db-wal`) and shared-memory file (`pulseport.db-shm`) are created alongside the main database file.

---

## SQLite Pragmas

Applied on every database open:

| Pragma | Value | Purpose |
|--------|-------|---------|
| `journal_mode` | `WAL` | Concurrent reads during writes |
| `synchronous` | `NORMAL` | Durable writes without full fsync per transaction |
| `mmap_size` | `67108864` (64 MB) | Memory-mapped I/O for read performance |
| `cache_size` | `-16384` (16 MB) | In-memory page cache |

---

## Migration System

Migrations are SQL files in `db/migrations/`, applied in filename order:

```
db/migrations/
└── 001_initial.sql
```

**Rules:**
- Files must be named `NNN_description.sql` (3-digit number prefix)
- Applied exactly once, tracked by `schema_version.version`
- Executed within a transaction
- `-- UP` section is run; `-- DOWN` section is commented out (for reference)

---

## Table Reference

### `schema_version`

Tracks which migrations have been applied.

```sql
CREATE TABLE schema_version (
    version INTEGER NOT NULL
);
```

| Column | Type | Description |
|--------|------|-------------|
| `version` | INTEGER | Highest applied migration number |

---

### `app_config`

Key-value store for runtime configuration persisted in the database.

```sql
CREATE TABLE app_config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);
```

| Column | Type | Description |
|--------|------|-------------|
| `key` | TEXT (PK) | Configuration key |
| `value` | TEXT | Configuration value (stored as string) |
| `updated_at_utc` | TEXT | ISO 8601 timestamp of last update |

**Seed values:**

| Key | Default Value |
|-----|---------------|
| `port` | `9770` |
| `log_level` | `info` |
| `retention_1m_days` | `90` |
| `retention_15m_days` | `365` |
| `retention_daily_days` | `365` |
| `retention_events_days` | `365` |

---

### `devices`

Device information (hostname, OS, hardware). One row per installation.

```sql
CREATE TABLE devices (
    id TEXT PRIMARY KEY,
    hostname TEXT NOT NULL,
    os_name TEXT NOT NULL,
    os_version TEXT NOT NULL,
    cpu_model TEXT,
    memory_bytes INTEGER,
    created_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    updated_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);
```

| Column | Type | Description |
|--------|------|-------------|
| `id` | TEXT (PK) | Unique device identifier (UUID or machine GUID) |
| `hostname` | TEXT | Machine hostname |
| `os_name` | TEXT | OS name (e.g., "Windows 11") |
| `os_version` | TEXT | OS build number |
| `cpu_model` | TEXT | CPU model string (nullable) |
| `memory_bytes` | INTEGER | Total physical RAM in bytes (nullable) |
| `created_at_utc` | TEXT | First seen timestamp |
| `updated_at_utc` | TEXT | Last updated timestamp |

---

### `metric_registry`

Static metadata for each registered metric key.

```sql
CREATE TABLE metric_registry (
    metric_key TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    unit TEXT NOT NULL,
    source TEXT NOT NULL,
    category TEXT NOT NULL
);
```

| Column | Type | Description |
|--------|------|-------------|
| `metric_key` | TEXT (PK) | Unique metric identifier (e.g., `cpu.total_pct`) |
| `display_name` | TEXT | Human-readable name (e.g., "CPU Total") |
| `unit` | TEXT | Unit of measurement (`%`, `MB`, `B/s`, `W`, `°C`, etc.) |
| `source` | TEXT | Data source (e.g., `pdh`, `system`, `wmi_acpi`) |
| `category` | TEXT | Grouping category (e.g., `cpu`, `memory`, `battery`) |

Populated at startup from collector `register_*` functions. Uses `INSERT OR REPLACE` — safe for restarts.

---

### `metric_current`

Most recent value for each metric. Updated via upsert on every sample tick.

```sql
CREATE TABLE metric_current (
    metric_key TEXT PRIMARY KEY,
    ts INTEGER NOT NULL,
    value_real REAL,
    unit TEXT NOT NULL,
    quality TEXT NOT NULL
);
```

| Column | Type | Description |
|--------|------|-------------|
| `metric_key` | TEXT (PK) | Metric identifier |
| `ts` | INTEGER | Unix epoch seconds when sampled |
| `value_real` | REAL | Current value |
| `unit` | TEXT | Unit of measurement |
| `quality` | TEXT | `measured`, `derived`, `estimated`, `unknown` |

**Write pattern:** `INSERT OR REPLACE` on every sample tick (1 second for core metrics). This table is always at most N rows (one per registered metric).

---

### `metric_1m`

1-minute aggregated metric data. This is the primary historical data table.

```sql
CREATE TABLE metric_1m (
    bucket_ts INTEGER NOT NULL,
    metric_key TEXT NOT NULL,
    min_value REAL,
    max_value REAL,
    avg_value REAL,
    sample_count INTEGER NOT NULL,
    quality TEXT NOT NULL,
    PRIMARY KEY (bucket_ts, metric_key)
);

CREATE INDEX idx_metric_1m_key_time ON metric_1m(metric_key, bucket_ts);
```

| Column | Type | Description |
|--------|------|-------------|
| `bucket_ts` | INTEGER (PK) | Bucket start time, floored to 60s boundary |
| `metric_key` | TEXT (PK) | Metric identifier |
| `min_value` | REAL | Minimum value seen in this minute |
| `max_value` | REAL | Maximum value seen in this minute |
| `avg_value` | REAL | Average value (sum / sample_count) |
| `sample_count` | INTEGER | Number of 1-second samples aggregated |
| `quality` | TEXT | Worst quality seen in the bucket |

**Write pattern:** Batch `INSERT OR REPLACE` every 60 seconds from the Aggregator. ~24 metrics × 1 row = 24 rows per flush.

**Retention:** 90 days (configurable).

**Index:** `(metric_key, bucket_ts)` for fast time-range queries per metric.

---

### `metric_15m`

15-minute aggregated metric data. Computed from `metric_1m` rows.

```sql
CREATE TABLE metric_15m (
    bucket_ts INTEGER NOT NULL,
    metric_key TEXT NOT NULL,
    min_value REAL,
    max_value REAL,
    avg_value REAL,
    sample_count INTEGER NOT NULL,
    quality TEXT NOT NULL,
    PRIMARY KEY (bucket_ts, metric_key)
);

CREATE INDEX idx_metric_15m_key_time ON metric_15m(metric_key, bucket_ts);
```

Schema is identical to `metric_1m` but with 900-second (15-minute) bucket boundaries.

**Write pattern:** Every 15 minutes, the Aggregator reads the last 15 rows of `metric_1m` per metric and computes:
- `min_value` = min of all 1m min values
- `max_value` = max of all 1m max values
- `avg_value` = weighted average (sum of avg × count) / total count
- `sample_count` = sum of all 1m sample counts
- `quality` = worst quality across all 1m buckets

**Retention:** 365 days (configurable).

---

### `energy_daily`

Daily energy consumption records. One row per calendar day.

```sql
CREATE TABLE energy_daily (
    day_local TEXT PRIMARY KEY,
    energy_wh REAL,
    avg_power_w REAL,
    peak_power_w REAL,
    charge_energy_wh REAL,
    discharge_energy_wh REAL,
    active_seconds INTEGER NOT NULL DEFAULT 0,
    quality TEXT NOT NULL DEFAULT 'unknown',
    finalized INTEGER NOT NULL DEFAULT 0,
    updated_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);
```

| Column | Type | Description |
|--------|------|-------------|
| `day_local` | TEXT (PK) | Local date (`YYYY-MM-DD`) |
| `energy_wh` | REAL | Total energy consumed in Watt-hours |
| `avg_power_w` | REAL | Average power draw in Watts |
| `peak_power_w` | REAL | Maximum instantaneous power in Watts |
| `charge_energy_wh` | REAL | Energy used for charging (AC → battery) |
| `discharge_energy_wh` | REAL | Energy drawn from battery |
| `active_seconds` | INTEGER | Seconds the system was active (not sleeping) |
| `quality` | TEXT | Data quality for the day |
| `finalized` | INTEGER | 1 if the day is complete, 0 if still accumulating |
| `updated_at_utc` | TEXT | Last update timestamp |

**Write pattern:** Accumulated throughout the day by the PowerPipeline. The row is updated (upserted) periodically and marked `finalized = 1` at the day boundary.

---

### `events`

System event log with auto-incrementing ID.

```sql
CREATE TABLE events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_ts INTEGER NOT NULL,
    severity TEXT NOT NULL,
    category TEXT NOT NULL,
    title TEXT NOT NULL,
    payload_json TEXT
);

CREATE INDEX idx_events_time ON events(event_ts);
CREATE INDEX idx_events_category_time ON events(category, event_ts);
```

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER (PK, autoincrement) | Unique event ID |
| `event_ts` | INTEGER | Event timestamp (Unix epoch seconds) |
| `severity` | TEXT | `info`, `warn`, `error`, `critical` |
| `category` | TEXT | Event category (e.g., `lifecycle`, `power`, `error`) |
| `title` | TEXT | Human-readable event description |
| `payload_json` | TEXT | Optional JSON payload (nullable) |

**Indexes:**
- `(event_ts)` — for time-range queries
- `(category, event_ts)` — for filtered time-range queries

**Write pattern:** Insert-only. Events are never updated. Deletion is via the API or retention cleanup.

---

### `deletions_audit`

Audit trail for all data deletion operations.

```sql
CREATE TABLE deletions_audit (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    scope TEXT NOT NULL,
    range_start TEXT,
    range_end TEXT,
    note TEXT
);
```

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER (PK, autoincrement) | Audit entry ID |
| `ts_utc` | TEXT | When the deletion occurred (ISO 8601) |
| `scope` | TEXT | Table name or `all_history` |
| `range_start` | TEXT | Start of deleted range (nullable, for all-history) |
| `range_end` | TEXT | End of deleted range (nullable) |
| `note` | TEXT | Human-readable note (e.g., "Deleted 1440 rows") |

This table is never cleaned up — it provides a permanent audit trail.

---

## Entity Relationship Diagram

```
┌──────────────────┐     ┌───────────────────┐
│  metric_registry │     │   metric_current   │
│                  │     │                    │
│  metric_key (PK) │◄────│  metric_key (PK)   │
│  display_name    │     │  ts                │
│  unit            │     │  value_real        │
│  source          │     │  unit              │
│  category        │     │  quality           │
└──────────────────┘     └───────────────────┘
         │
         │ (logical FK via metric_key)
         │
         ▼
┌──────────────────┐     ┌───────────────────┐
│    metric_1m     │     │    metric_15m      │
│                  │     │                    │
│  bucket_ts (PK)  │     │  bucket_ts (PK)    │
│  metric_key (PK) │     │  metric_key (PK)   │
│  min/max/avg     │────▶│  min/max/avg       │
│  sample_count    │     │  sample_count      │
│  quality         │     │  quality           │
└──────────────────┘     └───────────────────┘

┌──────────────────┐     ┌───────────────────┐
│   energy_daily   │     │      events        │
│                  │     │                    │
│  day_local (PK)  │     │  id (PK, auto)     │
│  energy_wh       │     │  event_ts          │
│  avg_power_w     │     │  severity          │
│  peak_power_w    │     │  category          │
│  active_seconds  │     │  title             │
│  finalized       │     │  payload_json      │
└──────────────────┘     └───────────────────┘

┌──────────────────┐     ┌───────────────────┐
│    app_config    │     │  deletions_audit   │
│                  │     │                    │
│  key (PK)        │     │  id (PK, auto)     │
│  value           │     │  ts_utc            │
│  updated_at_utc  │     │  scope             │
└──────────────────┘     │  range_start/end   │
                         │  note              │
┌──────────────────┐     └───────────────────┘
│  schema_version  │
│                  │
│  version         │
└──────────────────┘

┌──────────────────┐
│     devices      │
│                  │
│  id (PK)         │
│  hostname        │
│  os_name/version │
│  cpu_model       │
│  memory_bytes    │
└──────────────────┘
```

**Note:** SQLite does not enforce foreign keys by default. The relationships between `metric_registry` and the time-series tables are logical — metric_key values must match, but there is no `FOREIGN KEY` constraint.

---

## Storage Size Estimates

### Per-Row Sizes

| Table | Estimated Row Size | Notes |
|-------|-------------------|-------|
| `metric_current` | ~80 bytes | Key (30) + value (8) + unit (5) + quality (10) + ts (8) |
| `metric_1m` | ~90 bytes | Key (30) + 4 doubles (32) + count (4) + quality (10) + ts (8) |
| `metric_15m` | ~90 bytes | Same as metric_1m |
| `energy_daily` | ~120 bytes | Multiple double columns + strings |
| `events` | ~200 bytes | Variable (depends on title/payload length) |

### Growth Rate (24 Metrics)

| Table | Rows/Day | MB/Day | 90 Days | 365 Days |
|-------|----------|--------|---------|----------|
| `metric_1m` | 34,560 | ~3.0 | ~270 MB | — |
| `metric_15m` | 2,304 | ~0.2 | — | ~73 MB |
| `energy_daily` | 1 | <0.01 | — | <0.01 MB |
| `events` | ~20 | <0.01 | — | ~1.5 MB |

**Total at default retention:** ~350 MB with 24 metrics. More metrics increase size linearly.

### Maintenance

SQLite auto-manages page reuse after deletions. For manual compaction:

```sql
VACUUM;
```

WAL checkpoints happen:
- Automatically by SQLite when WAL exceeds 1000 pages
- Manually via `Database::checkpoint()`
- On database close

---

## Querying the Database Directly

The database can be queried with any SQLite client while PulsePort is running (WAL mode permits concurrent reads):

```powershell
# Using sqlite3 CLI
sqlite3 "C:\ProgramData\PulsePort\pulseport.db"
```

```sql
-- Last 10 minutes of CPU data
SELECT * FROM metric_1m
WHERE metric_key = 'cpu.total_pct'
  AND bucket_ts > unixepoch() - 600
ORDER BY bucket_ts DESC;

-- Today's energy
SELECT * FROM energy_daily
WHERE day_local = date('now', 'localtime');

-- Recent events
SELECT * FROM events
ORDER BY event_ts DESC
LIMIT 20;

-- Database size
SELECT page_count * page_size AS size_bytes
FROM pragma_page_count(), pragma_page_size();
```
