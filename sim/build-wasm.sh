#!/usr/bin/env bash
# Build the command center (C) to WASM and place it in flightbox/web/.
# Self-contained: osmmesh and the region PMTiles are VENDORED under sim/geo/ — no external
# checkout is required. The PMTiles are preloaded into MEMFS so the browser streams the world.
set -euo pipefail
cd "$(dirname "$0")"
: "${EMSDK_DIR:=$HOME/Git/emsdk}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

VEC="geo/data/hameln.pmtiles"
TER="geo/data/hameln_terrain.pmtiles"
./geo/fetch-data.sh        # cache: obtains the region PMTiles if they aren't there yet

emcc command_center/cc.c geo/osmmesh/src/*.c \
    -O2 -Icommon -Igeo/osmmesh/include \
    -DW3_RAD=2 -DW3_TEX=1024 -DW3_FARTEX=512 -DW3_TERR=22 \
    -sUSE_SDL=2 -lwebsocket.js -sFULL_ES2 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 \
    -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=128MB -sSTACK_SIZE=4MB \
    -sEXIT_RUNTIME=0 \
    --preload-file "$VEC@/hameln.pmtiles" \
    --preload-file "$TER@/hameln_terrain.pmtiles" \
    -o flightbox/web/cc.js
echo "WASM -> flightbox/web/cc.js (+ cc.wasm, cc.data with vendored Hameln PMTiles)"
