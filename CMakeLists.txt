cmake_minimum_required(VERSION 3.26)
project(cachestat)

set(CMAKE_CXX_STANDARD 17)

add_executable(cachestat main.cpp)
target_compile_options(cachestat PUBLIC -O0)
