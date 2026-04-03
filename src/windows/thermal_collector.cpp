#include "pulseport/collectors.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace pulseport {

#ifdef _WIN32

static bool s_thermal_available = false;
static IWbemLocator*  s_wbem_locator = nullptr;
static IWbemServices* s_wbem_services = nullptr;

void register_thermal_collectors(MetricRegistry& registry) {
    registry.register_metric({"temp.zone_c", "Thermal Zone", "°C", "wmi_acpi", "temperature"});

    // Initialize COM for WMI
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        spdlog::warn("COM init failed for thermal: 0x{:08X}", static_cast<unsigned>(hr));
        return;
    }

    hr = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr
    );
    // May fail if already called — that's OK

    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, reinterpret_cast<void**>(&s_wbem_locator));
    if (FAILED(hr)) {
        spdlog::warn("WbemLocator creation failed: 0x{:08X}", static_cast<unsigned>(hr));
        return;
    }

    hr = s_wbem_locator->ConnectServer(
        _bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr,
        &s_wbem_services
    );
    if (FAILED(hr)) {
        spdlog::info("WMI ROOT\\WMI not accessible — thermal monitoring unavailable");
        s_wbem_locator->Release();
        s_wbem_locator = nullptr;
        return;
    }

    // Test query to check availability
    IEnumWbemClassObject* enumerator = nullptr;
    hr = s_wbem_services->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &enumerator
    );
    if (SUCCEEDED(hr) && enumerator) {
        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
        if (SUCCEEDED(hr) && returned > 0) {
            s_thermal_available = true;
            obj->Release();
            spdlog::info("Thermal zone monitoring available");
        } else {
            spdlog::info("Thermal zone data not available on this hardware");
        }
        enumerator->Release();
    }
}

void collect_thermal(MetricRegistry& registry) {
    if (!s_thermal_available || !s_wbem_services) return;

    IEnumWbemClassObject* enumerator = nullptr;
    HRESULT hr = s_wbem_services->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &enumerator
    );
    if (FAILED(hr) || !enumerator) return;

    IWbemClassObject* obj = nullptr;
    ULONG returned = 0;
    hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
    if (SUCCEEDED(hr) && returned > 0) {
        VARIANT vtProp;
        hr = obj->Get(L"CurrentTemperature", 0, &vtProp, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            // WMI returns tenths of Kelvin
            double celsius = (static_cast<double>(vtProp.uintVal) / 10.0) - 273.15;
            int64_t ts = now_unix();
            registry.push_sample({"temp.zone_c", celsius, "°C", Quality::Measured, ts});
            VariantClear(&vtProp);
        }
        obj->Release();
    }
    enumerator->Release();
}

#else

void register_thermal_collectors(MetricRegistry&) {
    spdlog::warn("Thermal collectors not available on this platform");
}

void collect_thermal(MetricRegistry&) {}

#endif

} // namespace pulseport
