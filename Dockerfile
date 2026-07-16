# syntax=docker/dockerfile:1

# Latest Ubuntu as the base image.
FROM ubuntu:latest

LABEL description="Modern C++ development environment (recent GCC/G++, Make, CMake, GDB)"

# Avoid interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Etc/UTC

# ---------------------------------------------------------------------------
# 0. Trust the host's corporate root CAs.
#
#    This machine sits behind a Netskope TLS-inspection proxy that re-signs
#    HTTPS traffic with a private root CA. Without trusting it, any HTTPS call
#    inside the container (add-apt-repository -> launchpad.net, wget -> Kitware)
#    fails with:
#       SSL: CERTIFICATE_VERIFY_FAILED / self-signed certificate in chain
#
#    The .crt files in .devcontainer/certs/ are exported from the Windows
#    root store by .devcontainer/export-certs.ps1. Installing them here (before
#    any HTTPS access) makes apt, wget, git, python/openssl, etc. trust the
#    intercepting proxy.
# ---------------------------------------------------------------------------
RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        gnupg \
        wget \
        software-properties-common

# Copy the exported corporate/proxy root CAs into the system trust anchor dir.
# (Build context is the repo root, per .devcontainer/devcontainer.json.)
COPY .devcontainer/certs/ /usr/local/share/ca-certificates/

RUN set -eux; \
    update-ca-certificates

# Make common tooling pick up the system CA bundle explicitly, too.
ENV SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt \
    REQUESTS_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt \
    NODE_EXTRA_CA_CERTS=/etc/ssl/certs/ca-certificates.crt \
    GIT_SSL_CAINFO=/etc/ssl/certs/ca-certificates.crt

# ---------------------------------------------------------------------------
# 1. Third-party repositories for the freshest toolchain.
#    - ubuntu-toolchain-r/test : very recent GCC/G++ releases
#    - Kitware APT repo        : latest official CMake releases
#
#    These are best-effort: if the current Ubuntu codename is a pre-release
#    that the PPA/Kitware haven't published for yet, we simply fall back to the
#    (already recent) versions in Ubuntu's default repositories.
# ---------------------------------------------------------------------------
RUN set -eux; \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test || echo "WARN: toolchain PPA unavailable for this release; using default repos"; \
    if wget -qO - https://apt.kitware.com/keys/kitware-archive-latest.asc \
         | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg; then \
        . /etc/os-release; \
        echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ ${UBUNTU_CODENAME} main" \
            > /etc/apt/sources.list.d/kitware.list; \
    else \
        echo "WARN: could not fetch Kitware key; using default-repo cmake"; \
    fi; \
    apt-get update || echo "WARN: some apt sources failed to refresh; continuing"

# ---------------------------------------------------------------------------
# 2. Determine the newest available GCC major version and install it together
#    with the rest of the C++ development toolchain.
# ---------------------------------------------------------------------------
RUN set -eux; \
    GCC_VER="$(apt-cache search --names-only '^gcc-[0-9]+$' \
        | grep -oE 'gcc-[0-9]+' \
        | sed 's/gcc-//' \
        | sort -n \
        | tail -1)"; \
    echo "Installing GCC/G++ version: ${GCC_VER}"; \
    apt-get install -y --no-install-recommends \
        build-essential \
        make \
        cmake \
        gdb \
        git \
        openssh-client \
        pkg-config \
        ninja-build \
        python3-dev \
        debuginfod \
        elfutils \
        libc6-dbg \
        "libstdc++6-${GCC_VER}-dbg" \
        "gcc-${GCC_VER}" \
        "g++-${GCC_VER}"; \
    update-alternatives --install /usr/bin/gcc gcc "/usr/bin/gcc-${GCC_VER}" 100 \
        --slave /usr/bin/g++ g++ "/usr/bin/g++-${GCC_VER}"; \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# 3. Sanity check: print toolchain versions at build time.
# ---------------------------------------------------------------------------
RUN set -eux; \
    gcc --version; \
    g++ --version; \
    make --version; \
    cmake --version; \
    gdb --version

# ---------------------------------------------------------------------------
# 4. debuginfod: DISABLED by default.
#
#    gdb, when the debuginfod setting is *unset*, stops on an interactive prompt
#    on first launch:
#        "This GDB supports auto-downloading debuginfo from the following URLs:
#          <https://debuginfod.ubuntu.com>
#         Enable debuginfod for this session? (y or [n])"
#
#    Turning it ON (as a previous revision did) is worse than the prompt: the
#    moment gdb runs a program it loads libc/libstdc++ and makes a *synchronous*
#    HTTPS request to debuginfod.ubuntu.com to fetch their symbols. Behind the
#    corporate proxy that connection stalls with no reset and no timeout, so gdb
#    blocks forever on the first `run` — a hard hang that even SIGTERM can't
#    clear. That is the "gdb deadlocks when I run anything" symptom.
#
#    On top of that, debuginfod.ubuntu.com is itself currently unreachable even
#    from clean networks (it times out from every vantage point tested, while
#    the sibling servers debuginfod.elfutils.org / debuginfod.debian.net answer
#    instantly). And no other public server carries Ubuntu's build-ids, so there
#    is no working online replacement for this image's libraries anyway.
#
#    Setting it explicitly to OFF silences the interactive prompt (the prompt
#    only appears when the setting is unset) AND makes zero network calls, so
#    gdb runs programs instantly. Library debug symbols are instead provided
#    OFFLINE by the libc6-dbg / libstdc++6-*-dbg packages installed in step 2:
#    gdb reads them from /usr/lib/debug/.build-id/... with no network, so libc
#    and libstdc++ frames are fully symbolized. Symbols for the user's own -g
#    binaries are embedded in the ELF and need no download either.
#
#    Belt AND suspenders, because the two mechanisms cover different launchers:
#
#    (a) DEBUGINFOD_URLS is left EMPTY. With no server URL, gdb has nothing to
#        query no matter how the "enabled" setting ends up — this is the only
#        lever that survives every launcher. It is required because VS Code's
#        cppdbg starts gdb with `-nx`, which SKIPS /etc/gdb/gdbinit, so the
#        gdbinit line below never reaches the cppdbg MI session. Without an empty
#        URL, every F5 breakpoint stalled ~DEBUGINFOD_MAXTIME seconds on
#        unreachable lookups. debuginfod.ubuntu.com is unreachable anyway (see
#        above), so emptying it loses nothing today.
#
#    (b) /etc/gdb/gdbinit still sets 'enabled off' for terminal gdb, which DOES
#        read gdbinit — this also silences the interactive first-run prompt.
#
#    To opt back in (only worthwhile if/when debuginfod.ubuntu.com becomes
#    reachable AND you debug from a plain terminal), set DEBUGINFOD_URLS below
#    and change 'off' -> 'on' in the gdbinit line; for cppdbg you must also drop
#    the --init-command override in .vscode/launch.json. The DEBUGINFOD_*TIME*
#    env vars cap each request so a blocked proxy fails fast instead of hanging.
# ---------------------------------------------------------------------------
ENV DEBUGINFOD_URLS= \
    DEBUGINFOD_CACHE_PATH=/root/.cache/debuginfod_client \
    DEBUGINFOD_TIMEOUT=5 \
    DEBUGINFOD_MAXTIME=30

# Disable debuginfod for terminal gdb sessions (which read gdbinit) and silence
# the interactive first-run prompt. NOTE: cppdbg launches gdb with `-nx` and does
# NOT read this file — that path is covered by the empty DEBUGINFOD_URLS above
# plus the --init-command in .vscode/launch.json.
RUN set -eux; \
    mkdir -p /etc/gdb; \
    { \
        echo 'set debuginfod enabled off'; \
    } >> /etc/gdb/gdbinit

WORKDIR /workspace

CMD ["/bin/bash"]
