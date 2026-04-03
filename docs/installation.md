# Installation Guide

PulsePort can be installed via the MSI installer (recommended) or built from source.

---

## System Requirements

| Requirement | Minimum |
|-------------|---------|
| **Operating System** | Windows 10 version 1903 (May 2019 Update) or later |
| **Architecture** | x86-64 (AMD64) |
| **RAM** | 64 MB available |
| **Disk** | 100 MB (binary + database at default retention) |
| **Port** | 9770 (TCP, localhost only) |
| **Privileges** | Administrator (for service installation) |

---

## Method 1: MSI Installer (Recommended)

### Download

Download the latest `.msi` from the [GitHub Releases](https://github.com/hardikkanajariya-in/pulseport/releases) page.

Each release includes:
- `PulsePort-<version>-x64.msi` — Installer
- `checksums.sha256` — SHA-256 hash for verification

### Verify Download (Optional)

```powershell
# Check file hash
$expected = Get-Content checksums.sha256 | Select-String "PulsePort" | ForEach-Object { ($_ -split '\s+')[0] }
$actual = (Get-FileHash PulsePort-*.msi -Algorithm SHA256).Hash
if ($actual -eq $expected) { Write-Host "OK" } else { Write-Host "MISMATCH" }
```

### Install

1. Double-click the `.msi` file
2. Accept the license agreement (MIT)
3. Choose the installation directory (default: `C:\Program Files\PulsePort\`)
4. Click **Install** (requires Administrator)

The installer will:
- Copy the service binary, frontend files, and migration scripts
- Install the `PulsePort` Windows service (automatic start)
- Create the data directory at `C:\ProgramData\PulsePort\`
- Place a default `config.default.json` in the install directory
- Start the service automatically

### Verify Installation

```powershell
# Check service status
Get-Service PulsePort

# Test the API
Invoke-RestMethod http://127.0.0.1:9770/api/v1/health
```

Open `http://127.0.0.1:9770` in your browser to access the dashboard.

### Uninstall

1. Open **Settings → Apps → Installed apps**
2. Find **PulsePort** → Click **Uninstall**
3. Or via PowerShell:
   ```powershell
   Get-Package PulsePort | Uninstall-Package
   ```

The uninstaller:
- Stops and removes the Windows service
- Removes program files
- **Preserves** the database and logs in `C:\ProgramData\PulsePort\` (manual cleanup)

To fully remove all data:
```powershell
Remove-Item -Recurse -Force "C:\ProgramData\PulsePort"
```

---

## Method 2: Build from Source

### Prerequisites

| Tool | Version | Download |
|------|---------|----------|
| Visual Studio 2022 | 17.x with C++ workload | [visualstudio.com](https://visualstudio.com/) |
| CMake | 3.25+ | Bundled with VS, or [cmake.org](https://cmake.org/) |
| vcpkg | Latest | [github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg) |
| Node.js | 22+ | [nodejs.org](https://nodejs.org/) |
| pnpm | 10+ | `corepack enable && corepack prepare pnpm@latest --activate` |
| Git | Latest | [git-scm.com](https://git-scm.com/) |

### Setup vcpkg

```powershell
# Clone vcpkg (if not already installed)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Set environment variable
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
$env:VCPKG_ROOT = "C:\vcpkg"
```

### Clone and Build

```powershell
# Clone the repo
git clone https://github.com/hardikkanajariya-in/pulseport.git
cd pulseport

# Build frontend
cd web
pnpm install
pnpm exec vite build
cd ..

# Configure CMake (vcpkg manifest mode auto-installs dependencies)
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release --parallel

# Run tests
cd build
ctest --output-on-failure -C Release
cd ..
```

### Run Locally

```powershell
# Console mode (foreground)
.\build\apps\pulseport-service\Release\pulseport-service.exe --console

# With custom config
.\build\apps\pulseport-service\Release\pulseport-service.exe --console --config .\config.json
```

### Install as Service (Manual)

```powershell
# From an elevated (Administrator) PowerShell:
$exePath = (Resolve-Path .\build\apps\pulseport-service\Release\pulseport-service.exe).Path

# Create the service
New-Service -Name "PulsePort" `
  -BinaryPathName $exePath `
  -DisplayName "PulsePort Telemetry" `
  -Description "Local system telemetry with web dashboard" `
  -StartupType Automatic

# Start the service
Start-Service PulsePort
```

To remove the manually installed service:
```powershell
Stop-Service PulsePort
sc.exe delete PulsePort
```

---

## Method 3: Build MSI from Source

Requires [WiX Toolset v4](https://wixtoolset.org/) (dotnet tool):

```powershell
# Install WiX
dotnet tool install -g wix

# Build the MSI (after building the project)
cd packaging\windows
dotnet build PulsePort.wixproj -c Release

# Output: bin\Release\PulsePort-0.1.0-x64.msi
```

---

## Post-Installation

### Firewall

PulsePort binds to `127.0.0.1` by default and does **not** require any firewall rules. If you change the bind address to `0.0.0.0`, you'll need to create an inbound firewall rule:

```powershell
New-NetFirewallRule -DisplayName "PulsePort" `
  -Direction Inbound -LocalPort 9770 -Protocol TCP -Action Allow
```

### Windows Defender Exclusion (Optional)

If malware scans cause high disk I/O, consider excluding the database path:

```powershell
Add-MpPreference -ExclusionPath "C:\ProgramData\PulsePort"
```

### Service Management

```powershell
# View status
Get-Service PulsePort

# Stop
Stop-Service PulsePort

# Start
Start-Service PulsePort

# Restart (to apply config changes)
Restart-Service PulsePort

# View logs
Get-Content "C:\ProgramData\PulsePort\logs\pulseport.log" -Tail 50 -Wait
```

---

## Updating

### MSI Update

Download the new `.msi` and run it. The installer handles the upgrade in-place:
- Stops the service
- Replaces binaries
- Runs any new database migrations automatically on next start
- Starts the service

**Data is preserved** across updates. The database schema is migrated automatically.

### Source Update

```powershell
cd pulseport
git pull origin main

# Rebuild frontend
cd web
pnpm install
pnpm exec vite build
cd ..

# Rebuild backend
cmake --build build --config Release --parallel

# Restart service
Restart-Service PulsePort
```
