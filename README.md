# Matching Engine

A high-performance trading order matching engine that processes and executes trades based on matching buy and sell orders on an exchange.

## Overview

This project implements a matching engine that:
- Processes order requests (add/remove operations)
- Matches buy and sell orders based on price and time priority
- Executes trades when buy price ≥ sell price
- Maintains an order book with proper priority resolution
- Outputs trade execution details and order state changes

## Features

- **Price-Time Priority Matching**: Orders are matched at the best price first, with earliest orders matched first at the same price
- **Aggressive Order Handling**: Incoming orders that cross the spread are matched against resting orders in priority order
- **Order Book Management**: Maintains separate buy (descending price) and sell (ascending price) books
- **Partial Fill Support**: Orders can be partially filled with remaining quantity resting in the book

## Quick Start

```bash
# Build the matching engine
cmake -S . -B build -G Ninja
cmake --build build

# Run the matching engine
LD_LIBRARY_PATH=build/lib64 ./build/bin/matching_engine_app

# See example usage
bash QUICKSTART.sh
```

## Project Structure

- [matching_engine/matching_engine_core/](matching_engine/matching_engine_core) — Core matching engine library
- [matching_engine/matching_engine_app/](matching_engine/matching_engine_app) — Application and CLI interface
- [Matching_Engine_Requirement.md](Matching_Engine_Requirement.md) — Full specification and requirements
- [MATCHING_ENGINE_IMPLEMENTATION.md](MATCHING_ENGINE_IMPLEMENTATION.md) — Implementation details

---

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target matching_engine_app
```

## Run

```bash
LD_LIBRARY_PATH=build/lib64 ./build/bin/matching_engine_app
```

You can also run the guided script:

```bash
bash QUICKSTART.sh
```

## Debug (VS Code)

- Press F5 and choose `gdb: matching_engine_app`.
- The debugger launches [matching_engine/matching_engine_app/src/main.cpp](matching_engine/matching_engine_app/src/main.cpp).
- Debug settings are in [.vscode/launch.json](.vscode/launch.json).

## Input and Output

- Input format and matching rules: [Matching_Engine_Requirement.md](Matching_Engine_Requirement.md)
- Sample app behavior and test data: [matching_engine/matching_engine_app/README.md](matching_engine/matching_engine_app/README.md)

## Documentation Index

- [README.md](README.md) — Project summary and quick usage
- [VSCODE_DEVCONTAINER_SETUP.md](VSCODE_DEVCONTAINER_SETUP.md) — **How to set up and launch the development environment in VS Code**
- [RUNNING_TESTS.md](RUNNING_TESTS.md) — **How to run GoogleTest unit tests and the Python integration test**
- [BUILD_MATCHING_ENGINE.md](BUILD_MATCHING_ENGINE.md) — Build and environment notes
- [MATCHING_ENGINE_INDEX.md](MATCHING_ENGINE_INDEX.md) — Documentation index
- [MATCHING_ENGINE_IMPLEMENTATION.md](MATCHING_ENGINE_IMPLEMENTATION.md) — Design and implementation details
- [Matching_Engine_Requirement.md](Matching_Engine_Requirement.md) — Functional requirements and examples
- [matching_engine/matching_engine_app/README.md](matching_engine/matching_engine_app/README.md) — App-specific usage and examples

---

## References

This project is based on and inspired by the following open-source repositories by [@kingbirdogd](https://github.com/kingbirdogd):

- [kingbirdogd/super_make](https://github.com/kingbirdogd/super_make) — Original convention-over-configuration Makefile system
- [kingbirdogd/super_cmake](https://github.com/kingbirdogd/super_cmake) — CMake port of super_make (`common.mk`)
- [kingbirdogd/matching_sample](https://github.com/kingbirdogd/matching_sample) — Matching engine sample and specification
- [matching_engine/matching_engine_app/README.md](matching_engine/matching_engine_app/README.md) — App-specific usage and examples
