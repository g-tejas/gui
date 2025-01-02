cmake_minimum_required(VERSION 3.29)
project(untitled LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include(cmake/CPM.cmake)

add_executable(${CMAKE_PROJECT_NAME} main.cpp)
