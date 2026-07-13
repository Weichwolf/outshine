#!/usr/bin/env bash
# Build the command center (C) to WASM and place it in flightbox/web/.
set -euo pipefail
cd "$(dirname "$0")"
: "${EMSDK_DIR:=$HOME/Git/emsdk}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
emcc command_center/cc.c -O2 -Icommon \
    -sUSE_SDL=2 -lwebsocket.js -sALLOW_MEMORY_GROWTH \
    -o flightbox/web/cc.js
echo "WASM -> flightbox/web/cc.js (+ cc.wasm)"
