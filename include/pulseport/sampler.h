#pragma once

#include "pulseport/types.h"
#include "pulseport/metric_registry.h"

#include <atomic>
#include <functional>
#include <memory>

namespace pulseport {

/// Coordinated sampling scheduler.
/// Drives all collectors on a unified timer cadence.
class Sampler {
public:
    using CollectorFn = std::function<void(MetricRegistry&)>;

    explicit Sampler(MetricRegistry& registry);
    ~Sampler();

    /// Register a collector callback to run every `interval_ms` milliseconds.
    void add_collector(std::string name, uint32_t interval_ms, CollectorFn fn);

    /// Start the sampling thread. Non-blocking.
    void start();

    /// Signal stop and join the sampling thread.
    void stop();

    /// Set a callback that runs after every base tick (1s).
    void set_post_tick_callback(CollectorFn fn) { post_tick_ = std::move(fn); }

    bool running() const { return running_.load(std::memory_order_acquire); }

private:
    struct Collector {
        std::string  name;
        uint32_t     interval_ms;
        uint32_t     elapsed_ms = 0;
        CollectorFn  fn;
    };

    void run();

    MetricRegistry& registry_;
    std::atomic<bool> running_{false};
    std::vector<Collector> collectors_;
    CollectorFn post_tick_;
    void* timer_handle_ = nullptr;  // HANDLE (Windows waitable timer)
    void* thread_handle_ = nullptr; // std::jthread stored opaquely
};

} // namespace pulseport
