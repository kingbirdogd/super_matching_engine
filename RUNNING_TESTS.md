# Running Tests

This project has two layers of tests: **GoogleTest unit tests** (C++) and a **Python integration test** that exercises the compiled application end-to-end.

---

## GoogleTest Unit Tests

### Test suites

| Suite | Binary | What it tests |
|---|---|---|
| `MatchingEngineCoreTest` | `matching_engine_core_gtest` | `MatchingEngine` — typed `AddOrderRequest` / `CancelOrderRequest` APIs, order book logic, matching, priority |
| `LineMatchingEngineTest` | `line_matching_engine_gtest` | `LineMatchingEngine` — CSV line parsing, routing, error messages |

### Build the test binaries

From the project root inside the dev container:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target matching_engine_core_gtest line_matching_engine_gtest
```

> The test binaries are placed at:
> - `build/debug/bin/matching_engine_core_gtest`
> - `build/debug/bin/line_matching_engine_gtest`

### Run all tests with ctest

```bash
ctest --test-dir build --output-on-failure
```

Expected output:

```
Test project /workspace/build
      Start  1: MatchingEngineCoreTest.RejectsInvalidAddRequests
 1/13 Test  #1: MatchingEngineCoreTest.RejectsInvalidAddRequests ..........   Passed
      ...
13/13 Test #13: LineMatchingEngineTest.CancelFlowThroughLineInterface .....   Passed

100% tests passed out of 13
```

### Run a single test binary directly

```bash
./build/debug/bin/matching_engine_core_gtest
./build/debug/bin/line_matching_engine_gtest
```

### Run a specific test case

Use the `--gtest_filter` flag:

```bash
# Run a single test
./build/debug/bin/matching_engine_core_gtest --gtest_filter=MatchingEngineCoreTest.PricePriorityAcrossLevels

# Run all tests in one suite
./build/debug/bin/line_matching_engine_gtest --gtest_filter=LineMatchingEngineTest.*
```

### List all available tests

```bash
./build/debug/bin/matching_engine_core_gtest --gtest_list_tests
./build/debug/bin/line_matching_engine_gtest --gtest_list_tests
```

---

## Python Integration Test

The Python test runs the compiled application binary with a sample input file and checks that stdout and stderr match the expected outputs.

### Test data location

| File | Purpose |
|---|---|
| [`matching_engine/matching_engine_app/data/sample_input.txt`](matching_engine/matching_engine_app/data/sample_input.txt) | Input fed to the app via stdin |
| [`matching_engine/matching_engine_app/data/expected_stdout.txt`](matching_engine/matching_engine_app/data/expected_stdout.txt) | Expected stdout |
| [`matching_engine/matching_engine_app/data/expected_stderr.txt`](matching_engine/matching_engine_app/data/expected_stderr.txt) | Expected stderr |

### Run the integration test

```bash
python3 matching_engine/matching_engine_app/data/test.py
```

Expected output:

```
✓ Sample from spec

Results: 1 passed, 0 failed
```

---

## Run Everything at Once

Build all test targets and run all tests in one command:

```bash
cmake --build build --target matching_engine_app matching_engine_core_gtest line_matching_engine_gtest \
  && ctest --test-dir build --output-on-failure \
  && python3 matching_engine/matching_engine_app/data/test.py
```

---

## Test Source Files

| File | Suite |
|---|---|
| [`tests/matching_engine_core_test.cpp`](tests/matching_engine_core_test.cpp) | `MatchingEngineCoreTest` |
| [`tests/line_matching_engine_test.cpp`](tests/line_matching_engine_test.cpp) | `LineMatchingEngineTest` |
| [`matching_engine/matching_engine_app/data/test.py`](matching_engine/matching_engine_app/data/test.py) | Python integration test |
| [`tests/CMakeLists.txt`](tests/CMakeLists.txt) | Test build configuration |

---

## Adding New Tests

1. Add a new `TEST_F` in the appropriate `.cpp` file (or create a new one under `tests/`).
2. If creating a new file, register it in [`tests/CMakeLists.txt`](tests/CMakeLists.txt) following the existing pattern.
3. Rebuild and run `ctest`.
