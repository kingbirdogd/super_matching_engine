#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# super_cmake: sibling-build helper.
#
# Wrap `cmake` so that a *configure* invocation without an explicit `-B`
# defaults its build directory to "<source-dir>_build" — a sibling folder next
# to the source tree, matching super_make's layout. For example, run from
# /workspace:
#
#     cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja .
#         -> cmake ... . -B /workspace_build
#
# This is done in the shell (not in CMakeLists.txt) on purpose: CMake reads an
# existing in-source CMakeCache.txt and validates the generator *before* any
# CMakeLists.txt runs, so a project-level redirect cannot avoid polluting the
# source tree or the "generator does not match" error. Injecting -B up front
# means an in-source cache is never created.
#
# Passthrough (never rewritten): --build, --install, --open, -E, -P,
# --preset, --workflow, --build-preset, --find-package, and any invocation that
# already specifies -B.
# ---------------------------------------------------------------------------

cmake() {
    local a

    # Non-configure modes -> run unchanged.
    for a in "$@"; do
        case "$a" in
            --build|--install|--open|--preset|--workflow|--build-preset|-E|-P|--find-package)
                command cmake "$@"; return $? ;;
        esac
    done

    # Find an explicit binary dir (-B) and the source dir.
    local have_b=0 src="" prev=""
    for a in "$@"; do
        case "$a" in
            -B|-B?*) have_b=1 ;;
            -S?*)    src="${a#-S}" ;;
        esac
        case "$a" in
            -*) : ;;                              # flag, or the value of a flag (via prev)
            *)
                case "$prev" in
                    -G|-D|-U|-C|-T|-A|-S|-B|--toolchain) : ;;   # value belonging to a flag
                    *) [ -z "$src" ] && src="$a" ;;             # positional source dir
                esac
                ;;
        esac
        case "$prev" in
            -S) src="$a" ;;                       # spaced form: -S <dir>
        esac
        prev="$a"
    done

    # Explicit -B given -> respect it.
    if [ "$have_b" -eq 1 ]; then
        command cmake "$@"; return $?
    fi

    # Default the build dir to <source>_build (source defaults to CWD).
    [ -n "$src" ] || src="."
    local abs
    abs="$(cd "$src" 2>/dev/null && pwd)"
    if [ -z "$abs" ]; then
        command cmake "$@"; return $?
    fi
    echo "cmake: no -B given -> using sibling build dir ${abs}_build" >&2
    command cmake "$@" -B "${abs}_build"
}
