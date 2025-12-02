cmake_minimum_required(VERSION 3.14)
project(VideoFingerprintSystem VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for optimization and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Find packages
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${SQLite3_INCLUDE_DIRS}
)

# Source files
set(CORE_SOURCES
    src/core/fingerprint_generator.cpp
)

set(DATABASE_SOURCES
    src/database/database_manager.cpp
)

set(MATCHER_SOURCES
    src/matcher/matcher_service.cpp
)

set(UTILS_SOURCES
    src/utils/thread_pool.cpp
)

set(MONITORING_SOURCES
    src/monitoring/metrics.cpp
)

# Create library
add_library(vfs_lib STATIC
    ${CORE_SOURCES}
    ${DATABASE_SOURCES}
    ${MATCHER_SOURCES}
    ${UTILS_SOURCES}
    ${MONITORING_SOURCES}
)

target_link_libraries(vfs_lib
    PUBLIC
        Threads::Threads
        ${SQLite3_LIBRARIES}
)

# Main executable
add_executable(vfs_demo src/main.cpp)
target_link_libraries(vfs_demo PRIVATE vfs_lib)

# Testing
enable_testing()

# Unit tests
add_executable(test_fingerprint tests/test_fingerprint.cpp)
target_link_libraries(test_fingerprint PRIVATE vfs_lib)
add_test(NAME FingerprintTest COMMAND test_fingerprint)

add_executable(test_database tests/test_database.cpp)
target_link_libraries(test_database PRIVATE vfs_lib)
add_test(NAME DatabaseTest COMMAND test_database)

add_executable(test_matcher tests/test_matcher.cpp)
target_link_libraries(test_matcher PRIVATE vfs_lib)
add_test(NAME MatcherTest COMMAND test_matcher)

# Benchmarks
add_executable(benchmark_performance benchmarks/benchmark_performance.cpp)
target_link_libraries(benchmark_performance PRIVATE vfs_lib)

add_executable(benchmark_concurrency benchmarks/benchmark_concurrency.cpp)
target_link_libraries(benchmark_concurrency PRIVATE vfs_lib)

# Installation
install(TARGETS vfs_demo DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS vfs_lib DESTINATION lib)

# Print configuration
message(STATUS "")
message(STATUS "=== Video Fingerprint System Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "SQLite3: ${SQLite3_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "==============================================")
message(STATUS "")
