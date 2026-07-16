# Command-Line Development Shell Guide

This project supports command-line development using Docker-based shell scripts.

## Prerequisite (Required)

**Docker Desktop is required** before using either script:
- Linux-style systems: install and run Docker Desktop
- Windows: install and run Docker Desktop

Verify Docker is available:

```bash
docker --version
```

---

## Linux-style systems (Linux/macOS/WSL)

Use the Bash script:

```bash
bash ./dev_shell.sh
```

What it does:
1. Exports host certs from `.devcontainer/export-certs.sh`
2. Builds the development image from `Dockerfile`
3. Removes any previous `super_cmake-dev-shell` container
4. Starts an interactive shell in the container at `/workspace`

---

## Windows (PowerShell)

Use the PowerShell script:

```powershell
./dev_shell.ps1
```

If script execution is blocked, run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
./dev_shell.ps1
```

What it does:
1. Checks Docker availability
2. Optionally runs `.devcontainer/export-certs.sh` when `bash` is available
3. Builds the same development image from `Dockerfile`
4. Removes any previous `super_cmake-dev-shell` container
5. Starts an interactive shell in the container at `/workspace`

---

## Notes

- Keep Docker Desktop running while using the development shell.
- The workspace root is mounted into the container as `/workspace`.
- The container runs `.devcontainer/postCreate.sh` before opening the interactive shell.
