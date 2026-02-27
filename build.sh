#!/bin/bash
# Build script for MT5 Deal Processor
# Works on macOS and Linux without CMake

set -e

echo "Building MT5 Deal Processor..."

mkdir -p build

g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter \
    -Isrc -pthread \
    -o build/deal_processor \
    src/main.cpp \
    src/logger/Logger.cpp \
    src/mt_api/MockMTAPI.cpp \
    src/processor/DealProcessor.cpp \
    src/tracker/ResultTracker.cpp \
    src/client/ClientSimulator.cpp

echo "Build successful: build/deal_processor"
echo ""
echo "Usage:"
echo "  ./build/deal_processor          # Normal simulation (5 clients, 50 requests)"
echo "  ./build/deal_processor --burst   # High-frequency burst test (10 clients, 200 requests)"
