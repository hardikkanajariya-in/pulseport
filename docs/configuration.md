# Configuration Reference

PulsePort is configured via a JSON file. All fields have sensible defaults — a missing or empty config file results in a fully functional service.

---

## Config File Location

PulsePort searches for `config.json` in this order:

1. Path specified via `--config <path>` CLI flag
2. `config.json` in the current working directory
3. If neither exists, all defaults are used

When installed via MSI, the default config is placed at:
```
C:\Program Files\PulsePort\config.default.json
```

Runtime data (database, logs) is stored under:
```
C:\ProgramData\PulsePort\
```

---

## Full Configuration Schema

```json
{
  "host": "127.0.0.1",
  "port": 9770,
  "log_level": "info",
  "retention_1m_days": 90,
  "retention_15m_days": 365,
  "retention_daily_days": 365,
  "retention_events_days": 365,
  "sample_interval_ms": 1000,
  "process_interval_ms": 5000,
  "thermal_interval_ms": 5000,
  "aggregation_interval_s": 60
}
```

---

## Field Reference

### Network

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `host` | string | `"127.0.0.1"` | IP address to bind the HTTP server. **Must be `127.0.0.1` for security.** Changing to `0.0.0.0` will expose the dashboard to the network without authentication. |
| `port` | integer | `9770` | TCP port for the HTTP server and API. Must be between 1024 and 65535. |

### Logging

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `log_level` | string | `"info"` | Minimum log level. Options: `trace`, `debug`, `info`, `warn`, `error`. |

Log files are written to `<ProgramData>\PulsePort\logs\pulseport.log` with rotation:
- Max file size: 5 MB
- Max rotated files: 3
- Flush on `warn` or above
- Periodic flush every 3 seconds

**Log sinks:**

| Sink | When | Level |
|------|------|-------|
| Rotating file | Always | All (respects `log_level`) |
| Console (stdout) | Console mode only (`--console`) | All |
| Windows Event Log | Always | Critical only |

### Data Retention

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `retention_1m_days` | integer | `90` | Days to keep 1-minute aggregates. After this, older data is eligible for cleanup. |
| `retention_15m_days` | integer | `365` | Days to keep 15-minute aggregates. |
| `retention_daily_days` | integer | `365` | Days to keep daily energy records. |
| `retention_events_days` | integer | `365` | Days to keep system events. |

**Storage estimation at default retention:**

| Table | Bucket Interval | Rows/Day | 90 Days | 365 Days |
|-------|----------------|----------|---------|----------|
| `metric_1m` | 60s | ~34,560 (24 metrics × 1440 min) | ~3.1M rows | — |
| `metric_15m` | 900s | ~2,304 (24 × 96) | — | ~840K rows |
| `energy_daily` | 1 day | 1 | — | 365 rows |
| `events` | N/A | ~10-50 | — | ~18K rows |

Estimated database size: **50-150 MB** at full retention with default metrics.

### Sampling Intervals

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sample_interval_ms` | integer | `1000` | How often PDH and battery collectors run (milliseconds). Minimum recommended: 500. |
| `process_interval_ms` | integer | `5000` | How often the top-process collector runs. More expensive than other collectors due to PDH enumeration. |
| `thermal_interval_ms` | integer | `5000` | How often thermal zone data is collected via WMI. WMI queries are slow (~50-200ms). |
| `aggregation_interval_s` | integer | `60` | How often 1-minute aggregates are flushed to the database (seconds). Must align with 60s for correct bucket timestamps. |

### Auto-Resolved Paths

These fields are resolved automatically at runtime and are generally not set in the config file:

| Field | Auto-Resolved Value | Description |
|-------|---------------------|-------------|
| `db_path` | `C:\ProgramData\PulsePort\pulseport.db` | SQLite database file path |
| `log_dir` | `C:\ProgramData\PulsePort\logs\` | Log file directory |
| `web_dir` | `<exe_dir>\web\` | Frontend static files directory |
| `migrations_dir` | `<exe_dir>\db\migrations\` | SQL migration files directory |

If you need to override these, add them to the config JSON:
```json
{
  "db_path": "D:\\custom\\path\\pulseport.db",
  "log_dir": "D:\\custom\\logs\\",
  "web_dir": "D:\\custom\\web\\",
  "migrations_dir": "D:\\custom\\migrations\\"
}
```

---

## Default Config File

The MSI installer includes a default config at `config.default.json`:

```json
{
  "http_port": 9770,
  "bind_address": "127.0.0.1",
  "sample_interval_sec": 1,
  "retention_1m_days": 90,
  "retention_15m_days": 365,
  "retention_event_days": 365,
  "log_level": "info",
  "log_max_size_mb": 10,
  "log_max_files": 5,
  "db_path": "",
  "process_collector_enabled": true,
  "process_top_n": 10,
  "process_interval_sec": 5
}
```

> **Note:** The installer config uses slightly different field names (`http_port` vs. `port`, `bind_address` vs. `host`). The config parser accepts both forms for backward compatibility.

---

## CLI Arguments

The `pulseport-service` binary accepts these command-line flags:

| Flag | Short | Description |
|------|-------|-------------|
| `--console` | `-c` | Run in foreground (console mode) instead of as a Windows service |
| `--config <path>` |  | Path to `config.json` file |
| `--version` | `-v` | Print version and exit |
| `--help` | `-h` | Print usage help and exit |
| `--service` |  | Run as Windows service (this is the default) |

**Examples:**

```powershell
# Run in console mode with default config
pulseport-service.exe --console

# Run with a specific config file
pulseport-service.exe --console --config D:\myconfig.json

# Check version
pulseport-service.exe --version
```

---

## Environment

PulsePort does not read environment variables. All configuration is via the JSON file and CLI arguments.

---

## Hot Reload

Configuration is loaded once at startup. To apply changes:

1. Edit `config.json`
2. Restart the service:
   ```powershell
   Restart-Service PulsePort
   ```

Runtime config updates via the API (`POST /api/v1/config`) are planned but not yet implemented.

---

## Security Considerations

- **Never change `host` to `0.0.0.0`** unless you have a firewall or VPN in place. PulsePort has no authentication.
- The database file at `db_path` contains all collected metrics. Protect it with appropriate filesystem ACLs.
- Log files may contain sensitive system information (process names, network interface names). Apply appropriate file permissions.
- When running as a Windows service, PulsePort runs under the `LocalSystem` account by default. Consider using a dedicated service account with minimal privileges.
