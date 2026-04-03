#pragma once

#include "pulseport/metric_registry.h"

namespace pulseport {

/// Initialize all PDH-based collectors (CPU, memory, disk, network).
/// Registers metrics and returns collector callbacks for the Sampler.
void register_pdh_collectors(MetricRegistry& registry);

/// Collect current PDH counter values. Called by sampler on each tick.
void collect_pdh(MetricRegistry& registry);

/// Initialize battery/power collectors.
void register_battery_collectors(MetricRegistry& registry);
void collect_battery(MetricRegistry& registry);

/// Initialize thermal zone collectors (best-effort).
void register_thermal_collectors(MetricRegistry& registry);
void collect_thermal(MetricRegistry& registry);

/// Initialize process collectors (top-10, 5s interval).
void register_process_collectors(MetricRegistry& registry);
void collect_processes(MetricRegistry& registry);

} // namespace pulseport
