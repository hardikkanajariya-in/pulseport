#include "pulseport/sampler.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <thread>

namespace pulseport {

Sampler::Sampler(MetricRegistry& registry)
    : registry_(registry) {}

Sampler::~Sampler() {
    stop();
}

void Sampler::add_collector(std::string name, uint32_t interval_ms, CollectorFn fn) {
    collectors_.push_back({std::move(name), interval_ms, 0, std::move(fn)});
}

void Sampler::start() {
    if (running_.exchange(true)) return;

    spdlog::info("Sampler starting with {} collectors", collectors_.size());

#ifdef _WIN32
    timer_handle_ = CreateWaitableTimerExW(
        nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS
    );
    if (!timer_handle_) {
        // Fallback to standard timer if high-res not supported
        timer_handle_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
#endif

    auto* t = new std::jthread([this](std::stop_token) { run(); });
    thread_handle_ = t;
}

void Sampler::stop() {
    if (!running_.exchange(false)) return;

    spdlog::info("Sampler stopping");

#ifdef _WIN32
    if (timer_handle_) {
        CancelWaitableTimer(static_cast<HANDLE>(timer_handle_));
    }
#endif

    if (thread_handle_) {
        auto* t = static_cast<std::jthread*>(thread_handle_);
        t->request_stop();
        if (t->joinable()) t->join();
        delete t;
        thread_handle_ = nullptr;
    }

#ifdef _WIN32
    if (timer_handle_) {
        CloseHandle(static_cast<HANDLE>(timer_handle_));
        timer_handle_ = nullptr;
    }
#endif
}

void Sampler::run() {
    constexpr uint32_t kTickMs = 1000; // Base tick: 1 second

#ifdef _WIN32
    HANDLE timer = static_cast<HANDLE>(timer_handle_);
    LARGE_INTEGER due_time;
    due_time.QuadPart = -10000LL * kTickMs; // Negative = relative, in 100ns units

    if (!SetWaitableTimer(timer, &due_time, kTickMs, nullptr, nullptr, FALSE)) {
        spdlog::error("SetWaitableTimer failed: {}", GetLastError());
        running_ = false;
        return;
    }

    while (running_.load(std::memory_order_acquire)) {
        DWORD result = WaitForSingleObject(timer, kTickMs * 2);
        if (result != WAIT_OBJECT_0 || !running_.load(std::memory_order_acquire)) break;

        for (auto& col : collectors_) {
            col.elapsed_ms += kTickMs;
            if (col.elapsed_ms >= col.interval_ms) {
                col.elapsed_ms = 0;
                try {
                    col.fn(registry_);
                } catch (const std::exception& e) {
                    spdlog::error("Collector '{}' failed: {}", col.name, e.what());
                }
            }
        }
        if (post_tick_) {
            try { post_tick_(registry_); }
            catch (const std::exception& e) {
                spdlog::error("Post-tick callback failed: {}", e.what());
            }
        }
    }
#else
    // Non-Windows fallback (for future portability)
    while (running_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
        for (auto& col : collectors_) {
            col.elapsed_ms += kTickMs;
            if (col.elapsed_ms >= col.interval_ms) {
                col.elapsed_ms = 0;
                try {
                    col.fn(registry_);
                } catch (const std::exception& e) {
                    spdlog::error("Collector '{}' failed: {}", col.name, e.what());
                }
            }
        }
        if (post_tick_) {
            try { post_tick_(registry_); }
            catch (const std::exception& e) {
                spdlog::error("Post-tick callback failed: {}", e.what());
            }
        }
    }
#endif

    spdlog::info("Sampler thread exited");
}

} // namespace pulseport
