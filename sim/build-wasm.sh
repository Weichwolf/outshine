#!/usr/bin/env bash
# Build the command center (C) to WASM and place it in flightbox/web/.
# Self-contained: osmmesh is VENDORED under sim/geo/ — no external checkout required.
#
# NOTHING is preloaded. Every tile (terrain, OSM vector, imagery) is fetched on demand from the
# fb-tiles service at run time, which is what lets the aircraft start anywhere on earth instead
# of only over the one region that happened to be bundled. The browser is told where the service
# lives via window.FB_TILES_URL (see flightbox/server.c /config.js).
set -euo pipefail
cd "$(dirname "$0")"
: "${EMSDK_DIR:=$HOME/Git/emsdk}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

# -Wall -Wextra: this build ran with NO warnings at all, and it cost real bugs. An unterminated
# comment silently swallowed an `else` (the compiler says "/*" within comment -- nobody was
# listening), and a later fix for that line then failed to match the mangled text and vanished
# without a trace. -Wno-unused-parameter only because the GL/SDL callbacks are full of them.
emcc command_center/cc.c geo/osmmesh/src/*.c \
    -O2 -Wall -Wextra -Wno-unused-parameter -Icommon -Igeo/osmmesh/include \
    -sUSE_SDL=2 -lwebsocket.js -sFULL_ES2 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 \
    -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=128MB -sSTACK_SIZE=4MB \
    -sEXIT_RUNTIME=0 \
    -o flightbox/web/cc.js
echo "WASM -> flightbox/web/cc.js (+ cc.wasm) — no bundled region; tiles come from fb-tiles"
