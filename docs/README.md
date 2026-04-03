# PulsePort Documentation

Comprehensive documentation for PulsePort — a real-time Windows system telemetry service with a local web dashboard.

---

## Guides

| Document | Description |
|----------|-------------|
| [Installation](installation.md) | System requirements, MSI install, building from source, service management |
| [Configuration](configuration.md) | All config fields, defaults, CLI arguments, path resolution, security notes |
| [Development](development.md) | Dev environment setup, building, testing, debugging, code style, adding features |
| [Troubleshooting](troubleshooting.md) | Common issues, diagnostic steps, log reference, database maintenance |

## Reference

| Document | Description |
|----------|-------------|
| [Architecture](architecture.md) | System design, component diagram, data flow, concurrency model, tech stack |
| [API Reference](api-reference.md) | All REST endpoints with request/response examples, parameters, error codes |
| [Collectors](collectors.md) | PDH, battery, thermal, and process collectors — metric keys, data sources, intervals |
| [Database Schema](database-schema.md) | All tables, columns, indexes, ER diagram, storage estimates, direct query examples |
| [WebSocket Protocol](websocket-protocol.md) | Real-time data flow, polling fallback, message format, telemetry store |

---

## Quick Links

- **Dashboard:** `http://127.0.0.1:9770`
- **Health check:** `http://127.0.0.1:9770/api/v1/health`
- **GitHub:** [github.com/hardikkanajariya-in/pulseport](https://github.com/hardikkanajariya-in/pulseport)
- **Issues:** [github.com/hardikkanajariya-in/pulseport/issues](https://github.com/hardikkanajariya-in/pulseport/issues)
