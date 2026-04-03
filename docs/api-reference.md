# API Reference

PulsePort exposes a REST API on `http://127.0.0.1:9770/api/v1/`. All responses are `application/json`. All timestamps are **Unix epoch seconds** (UTC) unless noted otherwise.

---

## Base URL

```
http://127.0.0.1:9770/api/v1
```

The port is configurable via `config.json` (default: `9770`). The server binds to `127.0.0.1` only — it is not accessible from other machines.

---

## Authentication

None. PulsePort is a local-only tool. Security is enforced by binding to localhost.

---

## Common Response Format

Successful responses return HTTP 200 with a JSON body. Error responses use appropriate HTTP status codes:

| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad request (missing/invalid parameters) |
| 403 | Forbidden (CSRF origin check failed) |
| 500 | Internal server error |

Error response format:
```json
{
  "error": "Human-readable error description"
}
```

---

## Endpoints

### Health Check

```
GET /api/v1/health
```

Quick liveness probe. Returns service status, version, and uptime.

**Response:**
```json
{
  "status": "ok",
  "version": "0.1.0",
  "uptime": 3600
}
```

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | Always `"ok"` when service is running |
| `version` | string | PulsePort version (semver) |
| `uptime` | integer | Uptime in seconds since service start |

---

### System Information

```
GET /api/v1/system/info
```

Returns the list of all registered metrics and their metadata. Useful for building dynamic UIs.

**Response:**
```json
{
  "metrics": [
    {
      "key": "cpu.total_pct",
      "name": "CPU Total",
      "unit": "%",
      "category": "cpu"
    },
    {
      "key": "mem.available_mb",
      "name": "Memory Available",
      "unit": "MB",
      "category": "memory"
    }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `metrics[].key` | string | Unique metric identifier (dot-separated) |
| `metrics[].name` | string | Human-readable display name |
| `metrics[].unit` | string | Unit of measurement (`%`, `MB`, `B/s`, `W`, `°C`, etc.) |
| `metrics[].category` | string | Grouping category (`cpu`, `memory`, `disk`, `network`, `battery`, `power`, `temperature`, `process`) |

---

### Live Snapshot

```
GET /api/v1/live/snapshot
```

Returns the current value of every registered metric. This is the polling alternative to WebSocket.

**Response:**
```json
{
  "type": "snapshot",
  "tsUtc": 1743638400,
  "metrics": [
    {
      "key": "cpu.total_pct",
      "value": 23.5,
      "unit": "%",
      "quality": "measured",
      "ts": 1743638400
    },
    {
      "key": "mem.used_pct",
      "value": 67.2,
      "unit": "%",
      "quality": "measured",
      "ts": 1743638400
    }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | Always `"snapshot"` |
| `tsUtc` | integer | Server timestamp (Unix epoch seconds) |
| `metrics[].key` | string | Metric identifier |
| `metrics[].value` | number | Current value |
| `metrics[].unit` | string | Unit of measurement |
| `metrics[].quality` | string | Data quality: `"measured"`, `"derived"`, `"estimated"`, `"unknown"` |
| `metrics[].ts` | integer | When this sample was collected (Unix epoch seconds) |

**Data Quality Levels:**

| Quality | Meaning |
|---------|---------|
| `measured` | Direct hardware or OS reading |
| `derived` | Computed from other measurements (e.g., Watts from battery delta) |
| `estimated` | Heuristic approximation |
| `unknown` | Sensor unavailable or data missing |

---

### History Query

```
GET /api/v1/history?metric=<key>&start=<ts>&end=<ts>&resolution=<table>
```

Query aggregated historical data for a specific metric within a time range.

**Parameters:**

| Parameter | Required | Type | Description |
|-----------|----------|------|-------------|
| `metric` | Yes | string | Metric key (e.g., `cpu.total_pct`) |
| `start` | Yes | integer | Start of time range (Unix epoch seconds) |
| `end` | Yes | integer | End of time range (Unix epoch seconds) |
| `resolution` | No | string | `metric_1m` (default) or `metric_15m` |

**Response:**
```json
{
  "data": [
    {
      "key": "cpu.total_pct",
      "bucket_ts": 1743638400,
      "min": 12.3,
      "max": 98.7,
      "avg": 45.2,
      "sampleCount": 60,
      "quality": "measured"
    }
  ],
  "count": 1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `data[].key` | string | Metric identifier |
| `data[].bucket_ts` | integer | Bucket start time (Unix epoch seconds) |
| `data[].min` | number | Minimum value in bucket |
| `data[].max` | number | Maximum value in bucket |
| `data[].avg` | number | Weighted average value |
| `data[].sampleCount` | integer | Number of samples aggregated |
| `data[].quality` | string | Worst quality seen in bucket |
| `count` | integer | Total rows returned |

**Resolution guide:**

| Resolution | Bucket Size | Typical Use Case | Retention |
|-----------|-------------|-------------------|-----------|
| `metric_1m` | 60 seconds | Last 24 hours, detail views | 90 days |
| `metric_15m` | 900 seconds | Multi-day trends, overview | 365 days |

**Example:**
```
GET /api/v1/history?metric=cpu.total_pct&start=1743552000&end=1743638400&resolution=metric_1m
```

---

### Daily Energy

```
GET /api/v1/energy/daily?start=<YYYY-MM-DD>&end=<YYYY-MM-DD>
```

Query daily energy consumption records. Only available on devices with battery telemetry.

**Parameters:**

| Parameter | Required | Type | Description |
|-----------|----------|------|-------------|
| `start` | Yes | string | Start date (`YYYY-MM-DD`) |
| `end` | Yes | string | End date (`YYYY-MM-DD`) |

**Response:**
```json
{
  "data": [
    {
      "day": "2026-04-02",
      "energyWh": 45.6,
      "avgPowerW": 12.3,
      "peakPowerW": 65.0,
      "activeSeconds": 28800,
      "quality": "derived"
    }
  ],
  "count": 1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `data[].day` | string | Local date (`YYYY-MM-DD`) |
| `data[].energyWh` | number | Total energy consumed (Watt-hours) |
| `data[].avgPowerW` | number | Average power draw (Watts) |
| `data[].peakPowerW` | number | Peak power draw (Watts) |
| `data[].activeSeconds` | integer | Seconds the system was active |
| `data[].quality` | string | Data quality for the day |

---

### Events

```
GET /api/v1/events?start=<ts>&end=<ts>&category=<cat>
```

Query system events (service start/stop, errors, power state changes, etc.).

**Parameters:**

| Parameter | Required | Type | Description |
|-----------|----------|------|-------------|
| `start` | No | integer | Start time (Unix epoch seconds). Defaults to `0` |
| `end` | No | integer | End time (Unix epoch seconds). Defaults to now |
| `category` | No | string | Filter by category (e.g., `lifecycle`, `power`, `error`) |

**Response:**
```json
{
  "data": [
    {
      "id": 1,
      "ts": 1743638400,
      "severity": "info",
      "category": "lifecycle",
      "title": "PulsePort service started",
      "payload": {}
    }
  ],
  "count": 1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `data[].id` | integer | Auto-incrementing event ID |
| `data[].ts` | integer | Event timestamp (Unix epoch seconds) |
| `data[].severity` | string | `info`, `warn`, `error`, `critical` |
| `data[].category` | string | Event category |
| `data[].title` | string | Human-readable event title |
| `data[].payload` | object/string | Optional structured payload (parsed JSON or raw string) |

**Event Categories:**

| Category | Examples |
|----------|---------|
| `lifecycle` | Service started, service stopped |
| `power` | AC connected, AC disconnected, battery low |
| `error` | Collector failure, database error |
| `config` | Configuration changed |

---

### Diagnostics

```
GET /api/v1/diagnostics
```

Returns internal service diagnostics for the Diagnostics dashboard page.

**Response:**
```json
{
  "version": "0.1.0",
  "uptime": 86400,
  "lastSampleTime": 1743638400,
  "lastFlushTime": 1743638340,
  "wsConnections": 0,
  "dbSizeBytes": 5242880,
  "walSizeBytes": 131072,
  "registeredMetrics": 24,
  "serviceMode": "console"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `version` | string | PulsePort version |
| `uptime` | integer | Service uptime in seconds |
| `lastSampleTime` | integer | When the last metric sample was collected |
| `lastFlushTime` | integer | When the last 1-minute aggregation was flushed |
| `wsConnections` | integer | Number of active WebSocket connections |
| `dbSizeBytes` | integer | Main database file size |
| `walSizeBytes` | integer | WAL file size |
| `registeredMetrics` | integer | Number of registered metric keys |
| `serviceMode` | string | `"console"` or `"service"` |

---

### Delete History

```
POST /api/v1/history/delete
Content-Type: application/json
```

Delete historical data. Requires localhost origin (CSRF protection).

**CSRF Protection:** The `Origin` header must contain `127.0.0.1` or `localhost`. Requests from other origins receive HTTP 403.

#### Delete All History

**Request:**
```json
{
  "scope": "all"
}
```

**Response:**
```json
{
  "deleted": 15000
}
```

Deletes all data from: `metric_current`, `metric_1m`, `metric_15m`, `energy_daily`, `events`. Does NOT delete `metric_registry`, `app_config`, `devices`, or `schema_version`.

#### Delete by Time Range

**Request:**
```json
{
  "table": "metric_1m",
  "start": 1743552000,
  "end": 1743638400
}
```

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `table` | Yes | string | `metric_1m`, `metric_15m`, or `events` |
| `start` | Yes | integer | Start of range (Unix epoch seconds) |
| `end` | Yes | integer | End of range (Unix epoch seconds) |

**Response:**
```json
{
  "deleted": 1440
}
```

All deletions are recorded in the `deletions_audit` table with a timestamp, scope, range, and row count.

---

### Config Update (Placeholder)

```
POST /api/v1/config
Content-Type: application/json
```

**Status:** Placeholder — returns a stub response. Will be implemented with the Settings page to allow runtime configuration changes.

**Request:** Any valid JSON object.

**Response:**
```json
{
  "status": "ok",
  "message": "Config endpoint placeholder"
}
```

---

## Static File Serving

The HTTP server serves the Preact frontend as static files from the configured `web_dir` (default: `<exe_dir>/web/`). The mount point is `/`, so `GET /` serves `index.html`.

---

## Rate Limits

No explicit rate limiting is applied. Since the server is localhost-only, abuse scenarios are minimal. The 1 MB request body limit prevents accidental large payloads.

---

## Error Handling

All endpoints return consistent error JSON:

```json
{
  "error": "Missing required params: metric, start, end"
}
```

Parameter validation happens at the API boundary. Invalid table names, missing required parameters, and unparseable timestamps all return HTTP 400 with a descriptive message.
