cmake_minimum_required(VERSION 3.10)
project(EldenTree)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)

add_library(EldenTree STATIC src/EldenTree.cpp)
add_executable(main tests/main.cpp)
target_link_libraries(main EldenTree)