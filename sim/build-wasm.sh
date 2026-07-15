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

emcc command_center/cc.c geo/osmmesh/src/*.c \
    -O2 -Icommon -Igeo/osmmesh/include \
    -DW3_RAD=2 -DW3_TEX=1024 -DW3_FARTEX=512 -DW3_TERR=22 \
    -sUSE_SDL=2 -lwebsocket.js -sFULL_ES2 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 \
    -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=128MB -sSTACK_SIZE=4MB \
    -sEXIT_RUNTIME=0 \
    -o flightbox/web/cc.js
echo "WASM -> flightbox/web/cc.js (+ cc.wasm) — no bundled region; tiles come from fb-tiles"
