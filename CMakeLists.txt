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