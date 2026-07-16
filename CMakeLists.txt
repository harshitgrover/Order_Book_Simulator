cmake_minimum_required(VERSION 3.14)
project(HFTOrderBookSimulator VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enforce Release mode (for -O3 optimizations) by default so benchmarks are accurate
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

# Core engine library
add_library(engine src/engine/order_book.cpp)
target_include_directories(engine PUBLIC src)

# Simulation library
add_library(simulation src/simulation/noise_trader.cpp)
target_include_directories(simulation PUBLIC src)
target_link_libraries(simulation PRIVATE engine)

# Bot library
add_library(bots src/bots/avellaneda_stoikov_bot.cpp src/bots/stoikov_microprice_bot.cpp)
target_include_directories(bots PUBLIC src)
target_link_libraries(bots PRIVATE engine)

find_package(Threads REQUIRED)

# Main executable (CLI)
add_executable(simulator src/main.cpp)
target_link_libraries(simulator PRIVATE simulation bots engine Threads::Threads)

# 1. Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 2. Enable testing in CMake
enable_testing()

# 3. Create a test executable
add_executable(run_tests tests/test_order_book.cpp tests/test_bots.cpp)

# 4. Link your 'engine' library and Google Test
target_link_libraries(run_tests PRIVATE engine bots gtest_main)

# 5. Automatically discover tests
include(GoogleTest)
gtest_discover_tests(run_tests)

# --- Google Benchmark ---
FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable benchmark install" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(run_benchmarks tests/benchmark_order_book.cpp)
target_link_libraries(run_benchmarks PRIVATE engine bots benchmark::benchmark_main Threads::Threads)
