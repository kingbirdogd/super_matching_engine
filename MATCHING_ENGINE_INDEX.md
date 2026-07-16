# Matching Engine Project - Complete Index

## Quick Links

- **Implementation**: [matching_engine/matching_engine_core/](matching_engine/matching_engine_core/) (static library)
- **Application**: [matching_engine/matching_engine_app/](matching_engine/matching_engine_app/) (executable + tests)
- **Specification**: [Matching_Engine_Requirement.md](Matching_Engine_Requirement.md)
- **Build Guide**: [BUILD_MATCHING_ENGINE.md](BUILD_MATCHING_ENGINE.md)
- **Full Docs**: [matching_engine/matching_engine_app/README.md](matching_engine/matching_engine_app/README.md)
- **Quick Start**: [QUICKSTART.sh](QUICKSTART.sh)

## What You're Getting

A **production-quality C++23 matching engine** implementation that:

1. ✅ **Reads** CSV-formatted order messages (AddOrderRequest, CancelOrderRequest)
2. ✅ **Matches** buy and sell orders using price-then-time priority
3. ✅ **Outputs** trade events and order fill status updates
4. ✅ **Handles** errors gracefully (no crashes)
5. ✅ **Tests** successfully against the provided specification example
6. ✅ **Builds** with CMake + Ninja using SuperCMake conventions
7. ✅ **Documents** performance analysis and production improvements

## Getting Started in 3 Steps

### 1. Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target matching_engine_app
```

Or use the quick-start script:
```bash
./QUICKSTART.sh
```

### 2. Run Tests

```bash
python3 matching_engine/matching_engine_app/data/test.py
```

Expected: `✓ Sample from spec` → all tests pass

### 3. Try It Out

```bash
./build/debug/bin/matching_engine_app < matching_engine/matching_engine_app/data/sample_input.txt
```

Should output:
```
2,2,1025
4,1000008,1
3,1000005
2,1,1025
3,1000008
4,1000007,4
```

## Project Layout

```
workspace/
├── matching_engine/
│   ├── matching_engine_core/          ← Static library
│   │   ├── CMakeLists.txt
│   │   ├── inc/matching_engine_core/
│   │   │   └── matching_engine.hpp
│   │   └── src/
│   │       └── matching_engine.cpp
│   │
│   └── matching_engine_app/           ← Executable application
│       ├── CMakeLists.txt
│       ├── src/
│       │   └── main.cpp
│       └── data/
│           ├── sample_input.txt
│           ├── expected_stdout.txt
│           ├── expected_stderr.txt
│           └── test.py
│
├── Matching_Engine_Requirement.md     ← Original specification
├── BUILD_MATCHING_ENGINE.md           ← Full implementation summary
├── MATCHING_ENGINE_IMPLEMENTATION.md  ← Technical details
└── QUICKSTART.sh                      ← One-command setup
```

## Message Formats

### Input: AddOrderRequest

```csv
0,orderid,side,quantity,price
```

Example: `0,1000001,0,5,100` (buy 5 @ 100)

### Input: CancelOrderRequest

```csv
1,orderid
```

Example: `1,1000001` (cancel order 1000001)

### Output: TradeEvent

```csv
2,quantity,price
```

Example: `2,3,100` (trade 3 @ 100)

### Output: OrderFullyFilled

```csv
3,orderid
```

Example: `3,1000001` (order fully filled)

### Output: OrderPartiallyFilled

```csv
4,orderid,remaining_quantity
```

Example: `4,1000001,2` (order has 2 units remaining)

## Matching Algorithm

The engine matches orders in this order:

1. **Best price first**: 
   - For buy orders: match lowest sell prices first
   - For sell orders: match highest buy prices first

2. **Oldest first**: 
   - At the same price, match orders in FIFO order (insertion order)

3. **Partial fills**: 
   - If an order only partially matches, it becomes a resting order
   - If it fully matches, it's removed from the book

## Error Handling

Invalid input is logged to stderr; the program continues processing:

```
Unknown message type: BADMESSAGE
Invalid AddOrderRequest price: abc
Duplicate orderid: 123
```

No input will cause the program to crash.

## Documentation

| Document | Content |
| --- | --- |
| [Matching_Engine_Requirement.md](Matching_Engine_Requirement.md) | Full specification (logic, messages, example) |
| [BUILD_MATCHING_ENGINE.md](BUILD_MATCHING_ENGINE.md) | Implementation summary, performance analysis, production ideas |
| [matching_engine/matching_engine_app/README.md](matching_engine/matching_engine_app/README.md) | Build instructions, run examples, format details, performance table |

## Performance

| Operation | Time | Notes |
| --- | --- | --- |
| Add order (no match) | O(log m) | Insert into price level |
| Add order (matches) | O(n log m) | n = matched orders, m = price levels |
| Cancel order | O(log m) | Remove from level |

Typical book depth: 10–100 levels → very fast.

## C++23 Features Used

- `[[nodiscard]]` for error-prone return values
- `std::string_view` for zero-copy string passing
- `std::from_chars()` for fast integer parsing
- `std::contains()` for O(1) order lookup

## Testing

The project includes:
- **sample_input.txt**: 10 order messages + 1 bad message from the specification
- **expected_stdout.txt**: Correct output (6 trade/fill messages)
- **expected_stderr.txt**: Error message for bad input
- **test.py**: Automated test runner

All tests pass ✓.

## Build System

The project uses **SuperCMake**, a convention-over-configuration build system that mirrors the classic `common.mk` Makefile idiom:

```cmake
set(TYPE STATIC)              # or EXE, SHARE
set(STD c++23)                # C++ standard
set(VERSION 1.0.0)            # Version
super_module()                # Build it
```

This matches the existing project structure in the workspace.

## Next Steps

1. **Read** [BUILD_MATCHING_ENGINE.md](BUILD_MATCHING_ENGINE.md) for full details
2. **Run** `./QUICKSTART.sh` to verify everything works
3. **Explore** the code in [matching_engine/matching_engine_core/](matching_engine/matching_engine_core/)
4. **Extend** with additional test cases or features as needed

---

**Status**: ✅ **Ready to use**

Tested against the specification and passing all tests.
