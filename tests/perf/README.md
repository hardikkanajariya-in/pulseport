# Performance Tests

This directory contains performance benchmarks for PulsePort.

## Running

```bash
cmake --build build --config Release --target pulseport-perf-tests
.\build\Release\pulseport-perf-tests.exe
```

## Benchmarks

- **Ring Buffer Throughput** — Measures push/read ops per second
- **SQLite Write Throughput** — Measures batch insert performance
- **Aggregation Latency** — Measures time to compute 1m rollups

These benchmarks will be added as the codebase matures.
