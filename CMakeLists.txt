cmake_minimum_required(VERSION 3.3)
project(untitled4)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(SOURCE_FILES server.c)
add_executable(untitled4 ${SOURCE_FILES})