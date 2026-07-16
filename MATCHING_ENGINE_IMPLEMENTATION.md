# Matching Engine - Implementation Summary

## Overview

A modern C++23 matching engine matching buy and sell orders against a centralized order book, based on the SuperCMake build system architecture used in this workspace.

## Key Requirements Met

✓ **Architecture**: Core engine as static library (`matching_engine_core`), executable app (`matching_engine_app`)  
✓ **Language**: C++23 with SuperCMake integration  
✓ **Input**: CSV messages (AddOrderRequest, CancelOrderRequest)  
✓ **Output**: Trades, fill status updates to stdout; errors to stderr  
✓ **Matching Logic**: Price-then-time priority  
✓ **Error Handling**: Graceful, no crashes on bad input  
✓ **Testing**: Test suite with example dataset from spec  
✓ **Documentation**: Build, run, performance, production improvements  

## File Locations

### Core Library

- [matching_engine_core/CMakeLists.txt](matching_engine/matching_engine_core/CMakeLists.txt) — C++23 static library build config
- [matching_engine_core/inc/matching_engine_core/side.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/side.hpp) — `Side` enum
- [matching_engine_core/inc/matching_engine_core/output_type.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/output_type.hpp) — `OutputType` enum
- [matching_engine_core/inc/matching_engine_core/output_message.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/output_message.hpp) — `OutputMessage` struct
- [matching_engine_core/inc/matching_engine_core/process_result.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/process_result.hpp) — `ProcessResult` struct
- [matching_engine_core/inc/matching_engine_core/add_order_request.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/add_order_request.hpp) — `AddOrderRequest` struct
- [matching_engine_core/inc/matching_engine_core/cancel_order_request.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/cancel_order_request.hpp) — `CancelOrderRequest` struct
- [matching_engine_core/inc/matching_engine_core/matching_engine.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/matching_engine.hpp) — `MatchingEngine` class
- [matching_engine_core/inc/matching_engine_core/line_matching_engine.hpp](matching_engine/matching_engine_core/inc/matching_engine_core/line_matching_engine.hpp) — `LineMatchingEngine` class
- [matching_engine_core/src/matching_engine.cpp](matching_engine/matching_engine_core/src/matching_engine.cpp) — `MatchingEngine` implementation
- [matching_engine_core/src/line_matching_engine.cpp](matching_engine/matching_engine_core/src/line_matching_engine.cpp) — `LineMatchingEngine` CSV parsing

### Application

- [matching_engine_app/CMakeLists.txt](matching_engine_app/CMakeLists.txt) — executable build config
- [matching_engine_app/src/main.cpp](matching_engine_app/src/main.cpp) — stdin/stdout driver
- [matching_engine_app/README.md](matching_engine_app/README.md) — build, run, format, performance

### Test Data

- [matching_engine_app/data/sample_input.txt](matching_engine/matching_engine_app/data/sample_input.txt) — example from spec
- [matching_engine_app/data/expected_stdout.txt](matching_engine/matching_engine_app/data/expected_stdout.txt) — expected trades/fills
- [matching_engine_app/data/expected_stderr.txt](matching_engine/matching_engine_app/data/expected_stderr.txt) — expected errors
- [matching_engine_app/data/test.py](matching_engine/matching_engine_app/data/test.py) — Python integration test runner

### GoogleTest Suites

- [tests/matching_engine_core_test.cpp](tests/matching_engine_core_test.cpp) — unit tests for `MatchingEngine` typed API
- [tests/line_matching_engine_test.cpp](tests/line_matching_engine_test.cpp) — unit tests for `LineMatchingEngine` CSV parsing
- [tests/CMakeLists.txt](tests/CMakeLists.txt) — GoogleTest build configuration

## Building

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target matching_engine_app
```

Executable: `./build/debug/bin/matching_engine_app`

## Running

```bash
./build/debug/bin/matching_engine_app < input.txt
```

Run unit tests:
```bash
ctest --test-dir build --output-on-failure
```

Run Python integration test:
```bash
python3 matching_engine/matching_engine_app/data/test.py
```

See [RUNNING_TESTS.md](RUNNING_TESTS.md) for full details.

## Matching Algorithm

1. **Incoming order** arrives (buy or sell)
2. **Match phase**: iterate opposite side's best prices (highest buy or lowest sell first)
   - For each price level, iterate orders by insertion time (oldest first)
   - Trade the minimum of (incoming remaining, resting remaining)
   - Output TradeEvent + aggressive fill status + resting fill status
   - Remove or reduce resting order
3. **Resting phase**: if incoming has remaining qty, add to book at its price

## Example

**Input (stdin):**
```
0,1000000,1,1,1075      # Sell 1 @ 1075
0,1000001,0,9,1000      # Buy 9 @ 1000
...
0,1000008,0,3,1050      # Buy 3 @ 1050 → matches!
```

**Output (stdout):**
```
2,2,1025                # Trade 2 @ 1025 (older seller)
4,1000008,1             # Buyer partially filled (1 remaining)
3,1000005               # Seller fully filled
2,1,1025                # Trade 1 @ 1025 (newer seller)
3,1000008               # Buyer fully filled
4,1000007,4             # Newer seller partially filled (4 remaining)
```

## Performance

| Operation | Worst-Case | Typical |
| --- | --- | --- |
| Add order (no match) | O(log m) | O(log m) |
| Add order (matches n orders) | O(n + log m) | O(n + log m) |
| Cancel order | O(log m) | O(log m) |
| Match determination | Iterate opposite side | Immediate if no cross |

**n** = number of orders matched  
**m** = number of price levels

This is efficient for typical order book depths (10–100 levels).

## Production Enhancements

### Performance

- **Memory pooling**: pre-allocate order structures, reuse across operations
- **Lock-free data structures**: atomic queues for symbol-per-thread scalability
- **SIMD matching**: vectorize price comparisons in tight loops
- **Custom allocators**: reduce fragmentation for frequent alloc/free

### Architecture

- **Symbol routing**: separate book per symbol with independent matchers
- **Priority queues**: match best-priced levels before lower-priced ones
- **Index acceleration**: hash table of (symbol, price) → queue for O(1) level lookup

### Observability

- **Latency histograms**: track add/match/cancel latencies by quantile
- **Throughput metrics**: messages per second, trades per second
- **Book depth visualization**: active levels, order distribution by price
- **Audit trail**: persistent log of all executed trades for compliance

## Notes

- Prices are formatted as decimal strings with trailing zeros removed.
- Comments (lines with `//`) are stripped before parsing.
- Errors are logged to stderr; processing continues.
- Single-threaded by design (no mutexes).
- Static library approach allows easy embedding in larger systems.
