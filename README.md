# PulsePort

**Real-time Windows system telemetry with a local web dashboard.**

PulsePort is a lightweight Windows service that collects CPU, memory, disk, network, battery, thermal, power, and process metrics — then serves them through a local web dashboard and REST API.

## Features

- **Live Dashboard** — Real-time metric tiles with 1-second updates via WebSocket
- **Historical Data** — 1-minute and 15-minute aggregates stored in SQLite
- **Power Tracking** — Watt-hour accumulation and daily energy reports
- **Process Monitor** — Top-10 CPU consumers updated every 5 seconds
- **Zero Dependencies** — Single MSI installer, runs as a Windows service
- **Local Only** — All data stays on your machine, bound to `127.0.0.1:9770`

## Quick Start

### From Release

1. Download `PulsePort-x.y.z-win64.msi` from [Releases](../../releases)
2. Run the installer (requires admin for service registration)
3. Open `http://127.0.0.1:9770` in your browser

### From Source

**Prerequisites:** Visual Studio 2022 (C++20), CMake 3.25+, vcpkg, Node.js 22+

```bash
# Clone
git clone https://github.com/hardikkanajariya-in/pulseport.git
cd pulseport

# Build frontend
cd web && pnpm install && pnpm run build && cd ..

# Build backend
cmake --preset default
cmake --build build --config Release

# Run in console mode
.\build\Release\pulseport-service.exe --console
```

Open `http://127.0.0.1:9770` to see the dashboard.

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for a detailed overview.

```
┌──────────────────────────────────────────────────┐
│  Browser (Vite + Preact + uPlot)                 │
│  http://127.0.0.1:9770                           │
└────────────────┬─────────────────────────────────┘
                 │ HTTP/WebSocket
┌────────────────┴─────────────────────────────────┐
│  cpp-httplib Server                              │
│  REST API + WS + Static File Serving             │
├──────────────────────────────────────────────────┤
│  Metric Registry (lock-free ring buffers)        │
│  Sampler (WaitableTimer, 1s base tick)           │
│  Aggregator (1m / 15m / daily rollups)           │
├──────────────────────────────────────────────────┤
│  Collectors: PDH | Battery | Thermal | Process   │
├──────────────────────────────────────────────────┤
│  SQLite (WAL mode) + Migration Runner            │
├──────────────────────────────────────────────────┤
│  Windows Service (SCM integration)               │
└──────────────────────────────────────────────────┘
```

## API Reference

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/health` | Service health check |
| GET | `/api/v1/system/info` | OS & hardware info |
| GET | `/api/v1/live/snapshot` | Current metric values |
| GET | `/api/v1/history` | Historical aggregates |
| GET | `/api/v1/energy/daily` | Daily energy consumption |
| GET | `/api/v1/events` | System events log |
| GET | `/api/v1/diagnostics` | Service self-metrics |
| POST | `/api/v1/config` | Update configuration |
| POST | `/api/v1/history/delete` | Delete metric history |

## Configuration

Default config lives at `config.json` next to the executable (or `%ProgramData%\PulsePort\config.json` when installed as a service).

```json
{
  "http_port": 9770,
  "bind_address": "127.0.0.1",
  "sample_interval_sec": 1,
  "retention_1m_days": 90,
  "retention_15m_days": 365,
  "log_level": "info"
}
```

## System Requirements

- Windows 10 version 1903 or later (64-bit)
- ~50 MB disk for the application
- ~400 MB/year for metric history (with default retention)

## Author

**Hardik Kanajariya** — Full Stack Developer & Digital Solutions Expert

- Website: [hardikkanajariya.in](https://hardikkanajariya.in)
- GitHub: [@hardik-kanajariya](https://github.com/hardik-kanajariya)
- LinkedIn: [hardik-kanajariya](https://www.linkedin.com/in/hardik-kanajariya/)
- Twitter: [@hardik_web](https://x.com/hardik_web)
- Email: [hkdevs@hardikkanajariya.in](mailto:hkdevs@hardikkanajariya.in)

## License

MIT — see [LICENSE](LICENSE) for details.

---

Made with ❤️ in India by [Hardik Kanajariya](https://hardikkanajariya.in)
