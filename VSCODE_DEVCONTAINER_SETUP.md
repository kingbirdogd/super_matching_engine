# VS Code Dev Container Setup Guide

This guide walks you through setting up the full development environment for this project using **VS Code** and **Docker Desktop**. No manual compiler or CMake installation is needed — everything runs inside a pre-configured dev container.

---

## Prerequisites

### 1. Install VS Code

Download and install Visual Studio Code from:
https://code.visualstudio.com/

### 2. Install Docker Desktop

Download and install Docker Desktop from:
https://www.docker.com/products/docker-desktop/

- **Windows / macOS:** Install from the link above and start Docker Desktop before proceeding.
- **Linux:** Follow the [Docker Engine install guide](https://docs.docker.com/engine/install/) and ensure the Docker daemon is running (`sudo systemctl start docker`).

> Make sure Docker Desktop is **running** (visible in the system tray or menu bar) before opening VS Code.

### 3. Install the Dev Containers extension in VS Code

Open VS Code, go to the Extensions panel (`Ctrl+Shift+X` / `Cmd+Shift+X`), search for:

```
Dev Containers
```

Install the extension published by **Microsoft** (ID: `ms-vscode-remote.remote-containers`).

---

## Opening the Project

1. Clone or download this repository to your local machine.
2. Open VS Code.
3. Go to **File → Open Folder...** and select the project root folder (the one containing `Dockerfile` and `.devcontainer/`).

---

## Building the Dev Container

Once the folder is open in VS Code:

1. Open the Command Palette:
   - **Windows / Linux:** `Ctrl+Shift+P`
   - **macOS:** `Cmd+Shift+P`

2. Type and select:
   ```
   Dev Containers: Reopen in Container
   ```

3. VS Code will build the Docker image defined in the `Dockerfile` and reopen the workspace inside the container. This may take a few minutes on the first run while the image is being built.

4. Once complete, the bottom-left corner of VS Code will show a green label like:
   ```
   Dev Container: ...
   ```
   This confirms you are now working inside the container.

---

## Building the Project

### Option A — Default build task (Ctrl+Shift+B / Cmd+Shift+B)

Press `Ctrl+Shift+B` (`Cmd+Shift+B` on macOS) to run the default build task, which compiles `matching_engine_app` in Debug mode.

### Option B — Command Palette

1. Open the Command Palette (`Ctrl+Shift+P` / `Cmd+Shift+P`).
2. Type and select:
   ```
   Tasks: Run Build Task
   ```

### Option C — Terminal

Open the integrated terminal (`Ctrl+`` ` / `Cmd+`` `) and run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target matching_engine_app
```

---

## Debugging with F5

The project includes a pre-configured GDB debug launch configuration.

1. Open the file you want to set breakpoints in, e.g.:
   [`matching_engine/matching_engine_app/src/main.cpp`](matching_engine/matching_engine_app/src/main.cpp)

2. Click in the left gutter next to any line number to set a breakpoint (a red dot will appear).

3. Press **F5** to start debugging.
   - VS Code will automatically build the project first (via the pre-launch build task).
   - The debugger will launch `build/debug/bin/matching_engine_app` using GDB.
   - Execution will stop at `main()` (or your breakpoints).

4. Use the debug toolbar to:
   | Action | Key |
   |---|---|
   | Continue | `F5` |
   | Step Over | `F10` |
   | Step Into | `F11` |
   | Step Out | `Shift+F11` |
   | Stop | `Shift+F5` |

5. Inspect variables in the **Variables** panel (left sidebar), or hover over symbols in the editor.

> The debug configuration is defined in [`.vscode/launch.json`](.vscode/launch.json).

---

## Running Tests

### GoogleTest (unit tests)

Open the integrated terminal and run:

```bash
cmake --build build --target matching_engine_core_gtest line_matching_engine_gtest
ctest --test-dir build --output-on-failure
```

Expected output:
```
100% tests passed out of 13
```

### Python integration test

```bash
python3 matching_engine/matching_engine_app/data/test.py
```

Expected output:
```
✓ Sample from spec

Results: 1 passed, 0 failed
```

---

## Running the App Manually

Inside the container terminal:

```bash
echo "0,1,1,3,200
0,2,0,2,200" | ./build/debug/bin/matching_engine_app
```

Or run the guided quickstart script:

```bash
bash QUICKSTART.sh
```

---

## Project Layout (Dev Container)

```
/workspace/
├── matching_engine/
│   ├── matching_engine_core/       # Core library (hpp + cpp)
│   │   ├── inc/matching_engine_core/
│   │   └── src/
│   └── matching_engine_app/        # CLI application
│       ├── src/main.cpp
│       └── data/                   # Sample inputs and test.py
├── tests/                          # GoogleTest suites
├── build/                          # CMake build output (git-ignored)
├── .vscode/
│   ├── launch.json                 # F5 debug config
│   └── tasks.json                  # Build tasks
└── Dockerfile                      # Dev container definition
```

---

## Troubleshooting

| Problem | Solution |
|---|---|
| Docker not found | Make sure Docker Desktop is running before opening VS Code |
| Container fails to build | Check Docker has enough memory (≥ 2 GB) in Docker Desktop → Settings → Resources |
| F5 does nothing | Build the project first (`Ctrl+Shift+B`) then try F5 again |
| Breakpoints not hit | Ensure the build type is `Debug` (not `Release`) — check the CMake configure command |
| `gdb` hangs on start | This project uses a wrapper script (`.vscode/gdb-nodebuginfod.sh`) to prevent debuginfod network stalls — it is already configured |
