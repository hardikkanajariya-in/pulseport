# Contributing to PulsePort

Thank you for considering contributing to PulsePort! This document explains how to get started.

## Development Setup

1. **Prerequisites**
   - Visual Studio 2022 with C++ Desktop workload
   - CMake 3.25+
   - vcpkg (set `VCPKG_ROOT` environment variable)
   - Node.js 22+ and pnpm 10+
   - Git

2. **Clone and build**
   ```bash
   git clone https://github.com/hardikkanajariya-in/pulseport.git
   cd pulseport

   # Frontend
   cd web && pnpm install && pnpm run build && cd ..

   # Backend
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

3. **Run locally**
   ```bash
   .\build\Release\pulseport-service.exe --console
   ```

4. **Frontend dev server** (with API proxy to backend)
   ```bash
   cd web && pnpm run dev
   ```

## Code Style

- **C++**: C++20, follow existing patterns. Use `spdlog` for logging, `nlohmann::json` for JSON.
- **TypeScript**: Strictmode. Prefer Preact idioms (signals, hooks).
- **Naming**: `snake_case` for C++ files and functions, `kebab-case` for TypeScript files.

## Pull Request Process

1. Fork and create a feature branch from `main`
2. Make your changes with clear, focused commits
3. Ensure CI passes (backend builds + frontend builds)
4. Add tests for new functionality
5. Submit a PR with a clear description of what changed and why

## Reporting Issues

- Use GitHub Issues with a clear title
- Include your Windows version and PulsePort version
- For crashes, include the log file from `%ProgramData%\PulsePort\logs\`

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
