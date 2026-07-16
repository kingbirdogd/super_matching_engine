#!/bin/sh
# gdb wrapper used by .vscode/launch.json (miDebuggerPath).
#
# Why this exists: cpptools inspects gdb's environment and, when it sees
# DEBUGINFOD_URLS, explicitly issues `set debuginfod enabled on` during launch
# (AFTER setupCommands/--init-command run, so those can't win). gdb then makes a
# synchronous request per system library to debuginfod.ubuntu.com, which is
# unreachable behind the Netskope proxy and blocks ~15s each (~45s total) before
# every breakpoint. launch.json's "environment" only sets the *debuggee's* env
# (gdb's `set env`), not gdb's own, so it can't prevent this.
#
# Clearing the vars from gdb's OWN process environment here means: no URL to
# query, so cpptools' `set debuginfod enabled on` is a no-op and there are zero
# network calls. Offline debug symbols in /usr/lib/debug are still used.
unset DEBUGINFOD_URLS DEBUGINFOD_TIMEOUT DEBUGINFOD_MAXTIME DEBUGINFOD_CACHE_PATH
exec /usr/bin/gdb "$@"
