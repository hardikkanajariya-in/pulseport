# Development Guide

This guide covers setting up a development environment, building, testing, debugging, and contributing code to PulsePort.

---

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| Visual Studio 2022 | 17.x | C++20 compiler (MSVC) |
| CMake | 3.25+ | Build system |
| vcpkg | Latest | C++ dependency management |
| Node.js | 22+ | Frontend tooling |
| pnpm | 10+ | Frontend package manager |
| Git | Latest | Version control |

### VS 2022 Workloads

Install the following workloads via the Visual Studio Installer:
- **Desktop development with C++**
  - MSVC v143 build tools
  - Windows 10/11 SDK
  - C++ CMake tools for Windows

### Setup vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Set environment variable (permanent)
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
$env:VCPKG_ROOT = "C:\vcpkg"
```

### Setup pnpm

```powershell
# If using Node.js corepack:
corepack enable
corepack prepare pnpm@latest --activate

# Verify
pnpm --version  # Should be 10.x+
```

---

## Project Setup

```powershell
# Clone
git clone https://github.com/hardikkanajariya-in/pulseport.git
cd pulseport

# Install frontend dependencies
cd web
pnpm install
cd ..

# Configure CMake
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DPULSEPORT_BUILD_TESTS=ON

# Build
cmake --build build --config Debug --parallel
```

---

## Development Workflow

### Frontend Development

The frontend uses Vite with hot module replacement (HMR):

```powershell
cd web
pnpm exec vite --port 3000 --host 127.0.0.1
```

Vite proxies API requests to `127.0.0.1:9770` (the C++ backend). If the backend is not running, API calls will return 500 errors — the frontend still renders with placeholder data.

**Frontend structure:**

```
web/src/
├── app.tsx              # Router, page switching
├── main.tsx             # Preact render entry point
├── components/
│   ├── chart.tsx         # uPlot wrapper with ResizeObserver
│   ├── metric-tile.tsx   # Live metric display card
│   └── nav.tsx           # Sidebar navigation
├── hooks/
│   ├── use-api.ts        # REST client with refetch
│   └── use-websocket.ts  # WebSocket + polling fallback
├── lib/
│   └── telemetry-store.ts  # Preact Signals state store
├── pages/
│   ├── dashboard.tsx
│   ├── power.tsx
│   ├── history.tsx
│   ├── events.tsx
│   ├── settings.tsx
│   └── diagnostics.tsx
└── styles/
    └── global.css
```

**Key patterns:**
- **State management:** Preact Signals (`@preact/signals`) — no Redux or Context
- **Data fetching:** `useApi()` hook with automatic refetch interval
- **Charts:** uPlot with ResizeObserver for responsive sizing
- **Styling:** Plain CSS with CSS variables for theming (dark theme default)

### Backend Development

```powershell
# Build and run in console mode
cmake --build build --config Debug --parallel
.\build\apps\pulseport-service\Debug\pulseport-service.exe --console
```

**Debugging in VS 2022:**
1. Open `build/PulsePort.sln` or use CMake > Open Folder in VS
2. Set `pulseport-service` as the startup project
3. Add CLI arguments in Debug properties: `--console`
4. F5 to debug

**Debugging in VS Code:**
1. Install the C/C++ extension
2. `compile_commands.json` is generated in `build/` (enable `CMAKE_EXPORT_COMPILE_COMMANDS`)
3. Create a `.vscode/launch.json`:
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "PulsePort (Console)",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/apps/pulseport-service/Debug/pulseport-service.exe",
      "args": ["--console"],
      "cwd": "${workspaceFolder}"
    }
  ]
}
```

---

## Testing

### Running Tests

```powershell
cd build
ctest --output-on-failure -C Debug

# Or run specific test executables:
.\tests\Debug\pulseport-unit-tests.exe
.\tests\Debug\pulseport-integration-tests.exe
```

### Test Structure

```
tests/
├── CMakeLists.txt           # GoogleTest via FetchContent
├── unit/
│   ├── test_ring_buffer.cpp  # RingBuffer correctness
│   ├── test_types.cpp        # Type conversions, quality enum
│   └── test_migration_runner.cpp  # SQL migration parsing
├── integration/
│   ├── test_database.cpp     # SQLite open/close/checkpoint
│   └── test_storage_roundtrip.cpp  # Write → read cycle
└── perf/
    └── README.md             # Performance test guidelines
```

**Framework:** GoogleTest (fetched via CMake `FetchContent` — no manual install needed).

### Writing Tests

**Unit test example:**
```cpp
#include <gtest/gtest.h>
#include "pulseport/ring_buffer.h"

TEST(RingBufferTest, PushAndRead) {
    pulseport::RingBuffer<int, 4> buf;
    buf.push(1);
    buf.push(2);
    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf[0], 1);
    EXPECT_EQ(buf[1], 2);
}
```

**Integration test example:**
```cpp
#include <gtest/gtest.h>
#include "pulseport/database.h"

TEST(DatabaseTest, OpenAndClose) {
    pulseport::Database db;
    ASSERT_TRUE(db.open(":memory:", "../../db/migrations"));
    EXPECT_TRUE(db.is_open());
    db.close();
    EXPECT_FALSE(db.is_open());
}
```

### Frontend Tests

Currently, the frontend does not have automated tests. For visual testing, start the dev server and verify each page renders correctly:

```powershell
cd web
pnpm exec vite --port 3000
# Open http://127.0.0.1:3000 and check all 6 pages
```

---

## Code Style

### C++

- **Standard:** C++20
- **Naming:** `snake_case` for variables, functions, files; `PascalCase` for classes and types; `kCamelCase` for constants
- **Headers:** `#pragma once` (no include guards)
- **Namespaces:** All code in `namespace pulseport {}`
- **Includes:** System headers first, then library headers, then project headers
- **Formatting:** 4-space indent (enforced by `.editorconfig`)
- **Comments:** Doxygen-style `///` for public API, `//` for implementation notes

### TypeScript / Frontend

- **Framework:** Preact (not React — use `preact/hooks`, not `react`)
- **State:** Preact Signals — no Redux, MobX, or Context API
- **Formatting:** 2-space indent (enforced by `.editorconfig`)
- **Imports:** Named imports, no default exports except page components
- **CSS:** Plain CSS with variables — no Tailwind, SCSS, or CSS-in-JS

---

## Project Layout

```
pulseport/
├── apps/                     # Application entry points
│   └── pulseport-service/    # Windows service / console app
├── db/                       # Database
│   └── migrations/           # SQL migration files (numbered)
├── docs/                     # Documentation (you are here)
├── include/pulseport/        # Public C++ headers
├── packaging/windows/        # WiX MSI installer definition
├── src/                      # C++ source files
│   ├── config/               # Configuration manager
│   ├── core/                 # MetricRegistry, Sampler
│   ├── diagnostics/          # Self-metrics
│   ├── metrics/              # Aggregator, PowerPipeline
│   ├── server/               # HTTP server + routes
│   ├── storage/              # Database, Writer, Reader
│   └── windows/              # Platform-specific collectors
├── tests/                    # Automated tests
│   ├── unit/
│   ├── integration/
│   └── perf/
└── web/                      # Frontend (Vite + Preact)
```

### CMake Targets

| Target | Type | Description |
|--------|------|-------------|
| `pulseport-core` | STATIC lib | MetricRegistry, Sampler |
| `pulseport-storage` | STATIC lib | Database, Writer, Reader, MigrationRunner |
| `pulseport-server` | STATIC lib | HTTP server + API routes |
| `pulseport-config` | STATIC lib | Configuration manager |
| `pulseport-diagnostics` | STATIC lib | Self-metrics |
| `pulseport-metrics` | STATIC lib | Aggregator, PowerPipeline |
| `pulseport-windows` | STATIC lib | PDH, battery, thermal, process collectors |
| `pulseport-service` | EXECUTABLE | Entry point, links all above |
| `pulseport-unit-tests` | EXECUTABLE | GoogleTest unit tests |
| `pulseport-integration-tests` | EXECUTABLE | GoogleTest integration tests |

### Dependencies (vcpkg)

| Package | Purpose |
|---------|---------|
| `cpp-httplib` | HTTP server (header-only, v0.40+) |
| `sqlite3` | Embedded database |
| `spdlog` | Structured logging |
| `nlohmann-json` | JSON serialization/deserialization |

---

## Adding New Features

### Adding a New Metric Collector

1. **Register metrics** in `src/windows/your_collector.cpp`:
   ```cpp
   void register_your_collectors(MetricRegistry& registry) {
       registry.register_metric({"your.metric_key", "Display Name", "unit", "source", "category"});
   }
   ```

2. **Implement collection:**
   ```cpp
   void collect_your(MetricRegistry& registry) {
       int64_t ts = now_unix();
       double value = /* ... read from OS ... */;
       registry.push_sample({"your.metric_key", value, "unit", Quality::Measured, ts});
   }
   ```

3. **Declare in header** `include/pulseport/collectors.h`:
   ```cpp
   void register_your_collectors(MetricRegistry& registry);
   void collect_your(MetricRegistry& registry);
   ```

4. **Register in main.cpp:**
   ```cpp
   register_your_collectors(registry);
   sampler.add_collector("your", cfg.sample_interval_ms,
       [](MetricRegistry& r) { collect_your(r); });
   ```

5. **Add to CMakeLists.txt** in `src/CMakeLists.txt` under `pulseport-windows` sources.

### Adding a New API Endpoint

1. Add the route handler in `src/server/http_server.cpp` inside `setup_api_routes()`:
   ```cpp
   svr.Get("/api/v1/your/endpoint", [this](const httplib::Request& req, httplib::Response& res) {
       // ... handler logic ...
       res.set_content(body.dump(), "application/json");
   });
   ```

2. For mutating endpoints (`POST`/`DELETE`), add CSRF origin check.

3. Document the endpoint in `docs/api-reference.md`.

### Adding a New Database Migration

1. Create `db/migrations/002_your_change.sql`:
   ```sql
   -- Migration 002: Description
   -- UP
   ALTER TABLE ... ;
   ```

2. Migrations are applied automatically on startup in filename order.

### Adding a New Frontend Page

1. Create `web/src/pages/your-page.tsx`
2. Add the route in `web/src/app.tsx`
3. Add navigation link in `web/src/components/nav.tsx`

---

## CI / CD

See `.github/workflows/`:

| Workflow | Trigger | Steps |
|----------|---------|-------|
| `ci.yml` | Push to `main`, PRs | Build frontend → Build backend → Run tests |
| `release.yml` | Tag push `v*.*.*` | Build → Package MSI → Create GitHub Release |

Both workflows use `pnpm/action-setup@v4` for the frontend and `lukka/run-vcpkg@v11` + `lukka/run-cmake@v11` for the backend.
