cmake_minimum_required(VERSION 3.25)

# ── Test dependencies ──
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ── Unit Tests ──
add_executable(pulseport-unit-tests
  unit/test_ring_buffer.cpp
  unit/test_types.cpp
  unit/test_migration_runner.cpp
)

target_link_libraries(pulseport-unit-tests PRIVATE
  GTest::gtest_main
  pulseport-core
  pulseport-storage
)

target_include_directories(pulseport-unit-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

include(GoogleTest)
gtest_discover_tests(pulseport-unit-tests)

# ── Integration Tests ──
add_executable(pulseport-integration-tests
  integration/test_database.cpp
  integration/test_storage_roundtrip.cpp
)

target_link_libraries(pulseport-integration-tests PRIVATE
  GTest::gtest_main
  pulseport-core
  pulseport-storage
  pulseport-config
)

target_include_directories(pulseport-integration-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(pulseport-integration-tests)
