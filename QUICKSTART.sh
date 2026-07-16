#!/bin/bash
# Quick-start guide for the matching engine project

set -e

echo "=== Matching Engine Quick Start ==="
echo

cd "$(dirname "$0")"

echo "1. Configuring CMake..."
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1 || {
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
}

echo "2. Building matching_engine_app..."
cmake --build build --target matching_engine_app > /dev/null 2>&1 || {
    cmake --build build --target matching_engine_app
}

echo "3. Running test suite..."
python3 matching_engine/matching_engine_app/data/test.py

echo
echo "4. Quick manual test..."
echo "   Input: 0,100,0,5,100   # Buy 5 @ 100"
echo "          0,200,1,3,99     # Sell 3 @ 99"
echo "   Expected output:"
echo "   2,3,99                  # Trade 3 @ 99"
echo "   3,200                   # Seller fully filled"
echo "   4,100,2                 # Buyer partially filled (2 remaining)"
echo

echo "   0,100,0,5,100
    0,200,1,3,99"  | ./build/debug/bin/matching_engine_app

echo
echo "✓ Matching engine is ready!"
echo "  Binary: ./build/debug/bin/matching_engine_app"
echo "  Docs: matching_engine/matching_engine_app/README.md"
echo "  Tests: python3 matching_engine/matching_engine_app/data/test.py"
