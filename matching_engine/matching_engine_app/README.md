# Matching Engine Implementation

## Project Structure

```
matching_engine/
├── matching_engine_core/          # Static library (C++23)
│   ├── CMakeLists.txt
│   ├── inc/matching_engine_core/
│   │   └── matching_engine.hpp
│   └── src/
│       └── matching_engine.cpp
├── matching_engine_app/           # Executable application
│   ├── CMakeLists.txt
│   ├── src/
│   │   └── main.cpp
│   └── data/
│       ├── sample_input.txt       # Example input from spec
│       ├── expected_stdout.txt     # Expected output
│       ├── expected_stderr.txt     # Expected error messages
│       └── test.py                # Test runner
```

## Building

The project uses the SuperCMake build system (convention-over-configuration).

```bash
# Configure Debug build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build the matching engine app
cmake --build build --target matching_engine_app

# The executable is at:
./build/debug/bin/matching_engine_app
```

## Running

The matching engine reads CSV-formatted messages from stdin and writes results to stdout/stderr.

```bash
./build/debug/bin/matching_engine_app < input.txt
```

### Example

```bash
./build/debug/bin/matching_engine_app < matching_engine/matching_engine_app/data/sample_input.txt
```

Expected stdout:
```
2,2,1025
4,1000008,1
3,1000005
2,1,1025
3,1000008
4,1000007,4
```

Expected stderr (one line):
```
Unknown message type: BADMESSAGE
```

## Testing

Run the provided test suite:

```bash
cd /workspace
python3 example/matching_engine_app/data/test.py
```

## Input Format

Two message types are supported, identified by the first field (message type):

### AddOrderRequest (type 0)

```csv
0,orderid,side,quantity,price
```

- `orderid`: unique positive integer
- `side`: `0` = Buy, `1` = Sell
- `quantity`: positive integer
- `price`: decimal number > 0

**Example:** `0,1000001,0,9,1000` (add buy order 1000001 for 9 units at 1000)

### CancelOrderRequest (type 1)

```csv
1,orderid
```

- `orderid`: ID of order to cancel

**Example:** `1,1000004` (cancel order 1000004)

### Invalid input

Any line that does not conform to the above formats is rejected with an error message to stderr.
Comments (lines with `//`) are supported and stripped.

## Output Format

Three message types are written to stdout when orders are added:

### TradeEvent (type 2)

```csv
2,quantity,price
```

One line per matched order pair, at the resting order's price.

**Example:** `2,2,1025` (trade of 2 units at 1025)

### OrderFullyFilled (type 3)

```csv
3,orderid
```

Emitted when an order is completely filled and removed from the book.

**Example:** `3,1000005` (order 1000005 is fully filled)

### OrderPartiallyFilled (type 4)

```csv
4,orderid,quantity
```

Emitted when an order is partially filled and has remaining quantity.

**Example:** `4,1000008,1` (order 1000008 now has 1 unit remaining)

## Implementation Details

### Core Data Structures

- **Order book:** two maps (bids/asks) organized by price (time-ordered)
- **Order tracking:** unordered map for O(1) order lookup by ID
- **Matching:** greedy algorithm following the spec (best price, then oldest time)

### C++23 Features

- `[[nodiscard]]` attribute for error-prone functions
- `std::contains()` for efficient order lookup
- `std::from_chars()` for fast integer parsing
- `std::string_view` for efficient string handling

### Performance Characteristics

| Operation | Complexity | Notes |
| --- | --- | --- |
| AddOrderRequest matching | O(n log m) | n = matched orders, m = price levels |
| AddOrderRequest insertion | O(log m) | m = price levels |
| CancelOrderRequest | O(log m) | m = price levels |

- **Matching determination**: For each price level crossed, iterate the order queue (max queue length unbounded, but typically small for realistic data).
- **Filled order removal**: O(log m) level deletion, O(1) order map erase.
- **Cancel order removal**: O(log m) level lookup, O(1) order queue erase (stored iterator), O(1) order map erase.

### Production Improvements

If this were deployed at scale, consider:

1. **Allocator tuning**: custom allocator for order structures to reduce fragmentation.
2. **NUMA awareness**: pin threads and data to avoid remote memory access in high-frequency trading.
3. **Intrusive data structures**: use intrusive lists to eliminate separate storage.
4. **Lane-based concurrency**: separate order books per symbol with lock-free queues.
5. **Memory pooling**: pre-allocate order objects for fast reuse.
6. **Instruction cache**: organize hot-path code (matching loop) to fit L1.
7. **Metrics**: add latency histograms, throughput counters, and drift detection.

### Trade-Offs

- **Simplicity vs. Performance**: The current design prioritizes readable, maintainable code over micro-optimizations. Matching orders via iteration is O(n) per matched order, which is acceptable for typical order book depths (10–100 levels).
- **Memory vs. Latency**: Using separate order storage (map + queues) instead of intrusive structures trades extra pointer dereferences for easier reasoning about lifetime.
- **Single-threaded**: The implementation is single-threaded (no mutex/atomic needed). Multi-threading would require careful lock placement to avoid contention on the bid/ask maps.

## Error Handling

Malformed input is logged to stderr with a clear message and processing continues.

Example errors:

- `AddOrderRequest expects 5 fields`
- `Invalid AddOrderRequest orderid`
- `Invalid AddOrderRequest side`
- `Invalid AddOrderRequest quantity`
- `Invalid AddOrderRequest price`
- `Duplicate orderid: 123`
- `CancelOrderRequest expects 2 fields`
- `Invalid CancelOrderRequest orderid`
- `Unknown orderid for cancel: 123`
- `Unknown message type: BADMESSAGE`

## Notes

- Orders are fully matched before being added to the book if possible.
- If an aggressive order is partially matched, the remainder is added as a new resting order.
- Resting orders are prioritized by price (better first), then by insertion order (oldest first).
- All prices are stored as `long double` and printed with up to 18 significant digits (trailing zeros and decimal points removed).
