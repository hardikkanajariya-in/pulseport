#pragma once

#include <atomic>
#include <functional>
#include <string>

namespace pulseport {

/// Windows Service Control Manager integration.
/// Provides both --service (SCM) and --console (foreground) modes.
namespace service {

    /// Callback for the main application logic.
    /// Called after SCM reports SERVICE_RUNNING.
    /// Must block until shutdown is signaled, then return.
    using MainFn = std::function<void()>;

    /// Register the service entry point and start SCM dispatcher.
    /// This function does not return until the service is stopped.
    /// Call from main() when --service is passed.
    bool run_as_service(const std::string& service_name, MainFn main_fn);

    /// Run in console/foreground mode for development.
    /// Installs a Ctrl+C handler and calls main_fn.
    void run_as_console(MainFn main_fn);

    /// Signal the service to stop (called from HandlerEx or Ctrl+C).
    void signal_stop();

    /// Check if shutdown has been requested.
    bool stop_requested();

    /// Block until stop is signaled. Returns immediately if already stopped.
    void wait_for_stop();

} // namespace service
} // namespace pulseport
