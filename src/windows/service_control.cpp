#include "pulseport/service_control.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <csignal>
#endif

#include <condition_variable>
#include <mutex>

namespace pulseport::service {

// ── Shared state ────────────────────────────────────────────────

static std::atomic<bool> s_stop_requested{false};
static std::mutex s_stop_mutex;
static std::condition_variable s_stop_cv;
static MainFn s_main_fn;

void signal_stop() {
    s_stop_requested.store(true, std::memory_order_release);
    s_stop_cv.notify_all();
}

bool stop_requested() {
    return s_stop_requested.load(std::memory_order_acquire);
}

void wait_for_stop() {
    std::unique_lock lock(s_stop_mutex);
    s_stop_cv.wait(lock, [] { return s_stop_requested.load(); });
}

// ── Windows service implementation ──────────────────────────────

#ifdef _WIN32

static SERVICE_STATUS        s_svc_status{};
static SERVICE_STATUS_HANDLE s_svc_status_handle = nullptr;
static std::string           s_service_name;

static void report_status(DWORD state, DWORD exit_code = NO_ERROR,
                           DWORD wait_hint = 0) {
    static DWORD s_checkpoint = 1;

    s_svc_status.dwCurrentState  = state;
    s_svc_status.dwWin32ExitCode = exit_code;
    s_svc_status.dwWaitHint      = wait_hint;

    if (state == SERVICE_START_PENDING) {
        s_svc_status.dwControlsAccepted = 0;
    } else {
        s_svc_status.dwControlsAccepted =
            SERVICE_ACCEPT_STOP |
            SERVICE_ACCEPT_SHUTDOWN |
            SERVICE_ACCEPT_PRESHUTDOWN |
            SERVICE_ACCEPT_POWEREVENT;
    }

    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED) {
        s_svc_status.dwCheckPoint = 0;
    } else {
        s_svc_status.dwCheckPoint = s_checkpoint++;
    }

    SetServiceStatus(s_svc_status_handle, &s_svc_status);
}

static DWORD WINAPI service_handler_ex(
    DWORD control, DWORD event_type,
    LPVOID /*event_data*/, LPVOID /*context*/) {

    switch (control) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_PRESHUTDOWN:
            spdlog::info("Service stop requested (control={})", control);
            report_status(SERVICE_STOP_PENDING, NO_ERROR, 15000);
            signal_stop();
            return NO_ERROR;

        case SERVICE_CONTROL_POWEREVENT:
            if (event_type == PBT_APMRESUMEAUTOMATIC ||
                event_type == PBT_APMRESUMESUSPEND) {
                spdlog::info("System resumed from sleep/hibernate");
                // Collectors will detect the gap via timestamp delta
            }
            return NO_ERROR;

        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

static void WINAPI service_main(DWORD /*argc*/, LPWSTR* /*argv*/) {
    s_svc_status_handle = RegisterServiceCtrlHandlerExW(
        std::wstring(s_service_name.begin(), s_service_name.end()).c_str(),
        service_handler_ex,
        nullptr
    );

    if (!s_svc_status_handle) {
        spdlog::error("RegisterServiceCtrlHandlerEx failed: {}", GetLastError());
        return;
    }

    s_svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    report_status(SERVICE_START_PENDING, NO_ERROR, 5000);

    // Run the main application logic
    report_status(SERVICE_RUNNING);
    spdlog::info("Service reported RUNNING");

    if (s_main_fn) {
        s_main_fn();
    }

    report_status(SERVICE_STOPPED);
    spdlog::info("Service reported STOPPED");
}

bool run_as_service(const std::string& service_name, MainFn main_fn) {
    s_service_name = service_name;
    s_main_fn = std::move(main_fn);

    std::wstring wname(s_service_name.begin(), s_service_name.end());

    SERVICE_TABLE_ENTRYW dispatch_table[] = {
        {const_cast<LPWSTR>(wname.c_str()), service_main},
        {nullptr, nullptr}
    };

    // Configure preshutdown timeout
    SERVICE_PRESHUTDOWN_INFO psi = {};
    psi.dwPreshutdownTimeout = 10000; // 10 seconds
    // Note: This is set after the service handle is available (in service_main)

    if (!StartServiceCtrlDispatcherW(dispatch_table)) {
        DWORD err = GetLastError();
        if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            spdlog::warn("Not running as service (use --console for foreground mode)");
            return false;
        }
        spdlog::error("StartServiceCtrlDispatcher failed: {}", err);
        return false;
    }

    return true;
}

static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            spdlog::info("Console shutdown signal received");
            signal_stop();
            return TRUE;
        default:
            return FALSE;
    }
}

void run_as_console(MainFn main_fn) {
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    spdlog::info("Running in console mode (Ctrl+C to stop)");

    if (main_fn) {
        main_fn();
    }
}

#else
// ── Non-Windows fallback ────────────────────────────────────────

static void signal_handler(int) {
    signal_stop();
}

bool run_as_service(const std::string&, MainFn) {
    spdlog::error("Windows service mode not available on this platform");
    return false;
}

void run_as_console(MainFn main_fn) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    spdlog::info("Running in console mode (Ctrl+C to stop)");

    if (main_fn) {
        main_fn();
    }
}

#endif

} // namespace pulseport::service
