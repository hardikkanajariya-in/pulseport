cmake_minimum_required(VERSION 3.25)

# Read version from git tag or default
if(NOT DEFINED PULSEPORT_VERSION)
    set(PULSEPORT_VERSION "0.1.0")
endif()

project(PulsePort
    VERSION ${PULSEPORT_VERSION}
    DESCRIPTION "Local observability service for Windows"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- vcpkg dependencies ---
find_package(httplib CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# --- Global include path ---
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- Configure version header ---
configure_file(
    ${CMAKE_SOURCE_DIR}/include/pulseport/version.h.in
    ${CMAKE_SOURCE_DIR}/include/pulseport/version.h
    @ONLY
)

# --- Frontend dist directory ---
if(NOT DEFINED FRONTEND_BUILD_DIR)
    set(FRONTEND_BUILD_DIR "${CMAKE_SOURCE_DIR}/web/dist")
endif()
add_compile_definitions(PULSEPORT_WEB_DIR="${FRONTEND_BUILD_DIR}")

# --- Source libraries ---
add_subdirectory(src)

# --- Application ---
add_subdirectory(apps/pulseport-service)

# --- Tests ---
option(PULSEPORT_BUILD_TESTS "Build unit and integration tests" ON)
if(PULSEPORT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
