#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="super_cmake-dev-shell:latest"
CONTAINER_NAME="super_cmake-dev-shell"

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: docker is not installed or not on PATH." >&2
    exit 1
fi

echo "[1/4] Exporting host certs and creating optional mount files..."
bash "${ROOT_DIR}/.devcontainer/export-certs.sh"

echo "[2/4] Building Docker image: ${IMAGE_NAME}"
docker build \
    -f "${ROOT_DIR}/Dockerfile" \
    -t "${IMAGE_NAME}" \
    "${ROOT_DIR}"

echo "[3/4] Removing previous container (if exists): ${CONTAINER_NAME}"
docker rm -f "${CONTAINER_NAME}" >/dev/null 2>&1 || true

echo "[4/4] Starting development shell in container..."
exec docker run --rm -it \
    --name "${CONTAINER_NAME}" \
    --cap-add=SYS_PTRACE \
    --security-opt seccomp=unconfined \
    -v "${ROOT_DIR}:/workspace" \
    -v "${HOME}/.ssh:/root/.ssh-host:ro" \
    -v "${HOME}/.gitconfig:/root/.gitconfig-host:ro" \
    -v "${HOME}/.claude:/root/.claude" \
    -v "${HOME}/.claude.json:/root/.claude.json" \
    -w /workspace \
    "${IMAGE_NAME}" \
    bash -lc "bash .devcontainer/postCreate.sh && exec bash"
