#!/usr/bin/env bash
# Copyright 2026 Prosophor Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Stop llama-server by PID.
# Usage: stop_llamacpp_server.sh <pid> [--force]
#
# Exit codes:
#   0 = process stopped
#   1 = missing PID
#   2 = process not found

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Missing PID" >&2
    exit 1
fi

PID="$1"
FORCE=false

if [[ "${2:-}" == "--force" ]]; then
    FORCE=true
fi

if ! kill -0 "$PID" 2>/dev/null; then
    echo "Process $PID not found" >&2
    exit 2
fi

if $FORCE; then
    kill -9 "$PID" 2>/dev/null
else
    kill -TERM "$PID" 2>/dev/null
    sleep 0.5
    if kill -0 "$PID" 2>/dev/null; then
        kill -9 "$PID" 2>/dev/null
    fi
fi

exit 0
