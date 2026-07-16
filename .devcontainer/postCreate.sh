#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# postCreate: make git inside the container behave like it does on the host.
#
#   The host's ~/.ssh and ~/.gitconfig are bind-mounted read-only at
#   /root/.ssh-host and /root/.gitconfig-host (see "mounts" in
#   devcontainer.json). We copy them into the real locations here so that:
#     - SSH private keys get the strict 0600 perms ssh(1) demands (a read-only
#       bind mount would keep the host's perms, which ssh often rejects), and
#     - git config lives on a writable file the container can extend.
# ---------------------------------------------------------------------------
set -eu

# --- Host SSH keys --------------------------------------------------------
if [ -d /root/.ssh-host ]; then
    mkdir -p /root/.ssh
    # -a doesn't work across a mount boundary for perms we want to reset, so
    # copy contents then fix perms explicitly.
    cp -rf /root/.ssh-host/. /root/.ssh/ 2>/dev/null || true
    chmod 700 /root/.ssh
    # Everything defaults to 0600; public keys / known_hosts relax to 0644.
    find /root/.ssh -type f -exec chmod 600 {} + 2>/dev/null || true
    find /root/.ssh -type f \( -name '*.pub' -o -name 'known_hosts*' \) \
        -exec chmod 644 {} + 2>/dev/null || true
    echo "Installed host SSH keys into /root/.ssh"
else
    echo "WARN: /root/.ssh-host not mounted; skipping SSH key setup"
fi

# --- Host global git config ----------------------------------------------
if [ -f /root/.gitconfig-host ]; then
    cp -f /root/.gitconfig-host /root/.gitconfig
    echo "Installed host global git config into /root/.gitconfig"
else
    echo "WARN: /root/.gitconfig-host not mounted; skipping git config"
fi

# The workspace is a bind mount owned differently than the container user;
# without this git refuses to operate ("detected dubious ownership").
git config --global --add safe.directory /workspace || true

# --- Toolchain sanity check (original postCreate behavior) ----------------
gcc --version
g++ --version
cmake --version
gdb --version

# --- cmake sibling-build helper -------------------------------------------
# Make `cmake <configure>` without an explicit -B default its build dir to
# "<source>_build" (see cmake-sibling-build.sh). Sourced from the interactive
# shell so it applies in every new terminal.
_here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
_cmake_helper="${_here}/cmake-sibling-build.sh"
if [ -f "${_cmake_helper}" ]; then
    for _rc in /root/.bashrc /etc/bash.bashrc; do
        if ! grep -qF "${_cmake_helper}" "${_rc}" 2>/dev/null; then
            printf '\n# super_cmake sibling-build helper\n[ -f "%s" ] && . "%s"\n' \
                "${_cmake_helper}" "${_cmake_helper}" >> "${_rc}"
        fi
    done
    echo "Installed cmake sibling-build helper (source: ${_cmake_helper})"
fi

# --- gdb: avoid the container ASLR warning --------------------------------
# In a container gdb usually can't disable address-space randomization
# ("Error disabling address space randomization: Operation not permitted").
# Telling gdb not to try silences the warning. (The devcontainer also requests
# SYS_PTRACE + unconfined seccomp so it can actually disable it when allowed.)
if ! grep -qs 'disable-randomization' /root/.gdbinit 2>/dev/null; then
    printf 'set disable-randomization off\n' >> /root/.gdbinit
    echo "Configured /root/.gdbinit (disable-randomization off)"
fi
