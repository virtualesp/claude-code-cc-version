#!/usr/bin/env bash
# Start llama-server in background, output PID.
# Usage: start_llamacpp_server.sh <server> <model> <port> [ngl] [threads]

set -euo pipefail

SERVER="${1:?Usage: start_llamacpp_server.sh <server> <model> <port> [ngl] [threads]}"
MODEL="${2:?}"
PORT="${3:?}"
NGL="${4:-0}"
THREADS="${5:-0}"

# MinGW runtime DLLs
case "$(uname -s)" in MINGW*|MSYS*) export PATH="/e/devtool/msys64/mingw64/bin:$PATH";; esac

echo "[start_llamacpp_server.sh] starting: $SERVER -> $MODEL:$PORT" >&2

ARGS=(-m "$MODEL" --host 127.0.0.1 --port "$PORT" -c 4096)
[[ "$NGL" -eq -1 ]] && NGL=999
[[ "$NGL" -gt 0 ]] && ARGS+=(-ngl "$NGL")
[[ "$THREADS" -gt 0 ]] && ARGS+=(-t "$THREADS")

"$SERVER" "${ARGS[@]}" > /dev/null 2>&1 &
echo $!
