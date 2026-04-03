#include "pulseport/self_metrics.h"
#include "pulseport/version.h"

namespace pulseport {

static SelfMetrics s_instance;

SelfMetrics& self_metrics() {
    return s_instance;
}

int64_t SelfMetrics::uptime_seconds() const {
    if (start_time_unix == 0) return 0;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return now - start_time_unix;
}

} // namespace pulseport
