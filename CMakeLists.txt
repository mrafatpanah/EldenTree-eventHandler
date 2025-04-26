cmake_minimum_required(VERSION 3.10)
project(EldenTree)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)
add_library(EldenTree STATIC src/elden_tree/EldenTree.cpp)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(EldenTreeTest tests/test.cpp)
target_link_libraries(EldenTreeTest EldenTree gtest gtest_main)

add_executable(main examples/main.cpp)
target_link_libraries(main EldenTree)

# Google Benchmark
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.0
)

option(BUILD_BENCHMARKS "Build EldenTree benchmarks" OFF)

if(BUILD_BENCHMARKS)
    # Ensure Google Benchmark is available before adding target
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(elden_tree_benchmark benchmarks/EldenTreeBenchmark.cpp)

    # Link benchmark executable against your EldenTree library and Google Benchmark
    target_link_libraries(elden_tree_benchmark PRIVATE
        EldenTree 
        benchmark::benchmark
    )

    message(STATUS "Benchmarks enabled. Target: elden_tree_benchmark")
else()
     message(STATUS "Benchmarks disabled.")
endif()
