cmake_minimum_required(VERSION 3.16)
project(MT5DealProcessor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform-specific threading
find_package(Threads REQUIRED)

add_executable(deal_processor
    src/main.cpp
    src/logger/Logger.cpp
    src/mt_api/MockMTAPI.cpp
    src/processor/DealProcessor.cpp
    src/tracker/ResultTracker.cpp
    src/client/ClientSimulator.cpp
)

target_include_directories(deal_processor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(deal_processor PRIVATE Threads::Threads)

# Compiler warnings
target_compile_options(deal_processor PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
)
