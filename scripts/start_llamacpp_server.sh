#!/usr/bin/env bash
# Copyright 2026 Prosophor Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Start llama-server in background and output PID.
# Reads config from ~/.prosophor/config.json (local_models[0]).
#
# Uses exec so that LaunchDetachedCommand (which captures PID via $!)
# gets the correct llama-server PID.
#
# Exit codes:
#   0 = server started
#   1 = config/field missing
#   2 = server binary not found
#   3 = model file not found

set -euo pipefail

CONFIG_PATH="${HOME}/.prosophor/settings.json"
if [[ ! -f "$CONFIG_PATH" ]]; then
    echo "Config not found: $CONFIG_PATH" >&2
    exit 1
fi

# Parse config with jq (or fallback to python3)
if command -v jq &>/dev/null; then
    SERVER_PATH=$(jq -r '.local_models[0].server_path // empty' "$CONFIG_PATH")
    MODEL_PATH=$(jq -r '.local_models[0].model_path  // empty' "$CONFIG_PATH")
    PORT=$(jq -r '.local_models[0].port // "8080"' "$CONFIG_PATH")
    NGL=$(jq -r '.local_models[0].n_gpu_layers // "0"' "$CONFIG_PATH")
    THREADS=$(jq -r '.local_models[0].n_threads // "0"' "$CONFIG_PATH")
elif command -v python3 &>/dev/null; then
    read -r SERVER_PATH MODEL_PATH PORT NGL THREADS <<< "$(
        python3 -c "
import json,sys
c=json.load(open('$CONFIG_PATH'))
lm=c.get('local_models',[{}])[0]
print(lm.get('server_path','') or '')
print(lm.get('model_path','') or '')
print(lm.get('port',8080))
print(lm.get('n_gpu_layers',0))
print(lm.get('n_threads',0))
" | tr '\n' ' '
    )"
else
    echo "Neither jq nor python3 found — cannot parse config" >&2
    exit 1
fi

if [[ -z "$SERVER_PATH" ]]; then
    echo "local_models[0].server_path is empty in $CONFIG_PATH" >&2
    exit 1
fi
if [[ -z "$MODEL_PATH" ]]; then
    echo "local_models[0].model_path is empty in $CONFIG_PATH" >&2
    exit 1
fi
if [[ ! -x "$SERVER_PATH" ]]; then
    echo "Server binary not found or not executable: $SERVER_PATH" >&2
    exit 2
fi
if [[ ! -f "$MODEL_PATH" ]]; then
    echo "Model file not found: $MODEL_PATH" >&2
    exit 3
fi

# Build args array
ARGS=("-m" "$MODEL_PATH" "--port" "$PORT" "--host" "127.0.0.1" "-c" "4096")

if [[ "$NGL" -gt 0 ]]; then
    ARGS+=("-ngl" "$NGL")
elif [[ "$NGL" -eq -1 ]]; then
    ARGS+=("-ngl" "999")
fi

if [[ "$THREADS" -gt 0 ]]; then
    ARGS+=("-t" "$THREADS")
fi

# exec replaces shell with llama-server so PID tracking works natively
exec "$SERVER_PATH" "${ARGS[@]}"
