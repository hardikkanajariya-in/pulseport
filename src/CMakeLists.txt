# Core library
add_library(pulseport-core STATIC
    core/metric_registry.cpp
    core/sampler.cpp
)
target_link_libraries(pulseport-core PUBLIC spdlog::spdlog)

# Storage library
add_library(pulseport-storage STATIC
    storage/database.cpp
    storage/migration_runner.cpp
    storage/writer.cpp
    storage/reader.cpp
)
target_link_libraries(pulseport-storage PUBLIC
    unofficial::sqlite3::sqlite3
    spdlog::spdlog
)

# Server library
add_library(pulseport-server STATIC
    server/http_server.cpp
    server/api_routes.cpp
)
target_link_libraries(pulseport-server PUBLIC
    pulseport-core
    pulseport-storage
    httplib::httplib
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Config library
add_library(pulseport-config STATIC
    config/config_manager.cpp
)
target_link_libraries(pulseport-config PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Diagnostics
add_library(pulseport-diagnostics STATIC
    diagnostics/self_metrics.cpp
)
target_link_libraries(pulseport-diagnostics PUBLIC spdlog::spdlog)

# Metrics (aggregator, power pipeline)
add_library(pulseport-metrics STATIC
    metrics/aggregator.cpp
    metrics/power_pipeline.cpp
)
target_link_libraries(pulseport-metrics PUBLIC
    pulseport-core
    pulseport-storage
    spdlog::spdlog
)

# Windows collectors
add_library(pulseport-windows STATIC
    windows/pdh_collector.cpp
    windows/battery_collector.cpp
    windows/thermal_collector.cpp
    windows/process_collector.cpp
    windows/service_control.cpp
)
target_link_libraries(pulseport-windows PUBLIC
    pulseport-core
    spdlog::spdlog
    pdh
    wbemuuid
)
