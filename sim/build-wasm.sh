#!/usr/bin/env bash
# Build the command center (C) to WASM and place it in flightbox/web/.
# Links libosmmesh (OSM Shortbread + Copernicus terrain -> live meshes) and
# preloads the region PMTiles into MEMFS so the browser streams the real world.
set -euo pipefail
cd "$(dirname "$0")"
: "${EMSDK_DIR:=$HOME/Git/emsdk}"
: "${OSM_DIR:=$HOME/Git/wasm-osm}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

VEC="$OSM_DIR/wasm/data/hameln.pmtiles"
TER="$OSM_DIR/wasm/data/hameln_terrain.pmtiles"
[ -f "$VEC" ] && [ -f "$TER" ] || { echo "missing PMTiles under $OSM_DIR/wasm/data"; exit 1; }

emcc command_center/cc.c "$OSM_DIR"/libosmmesh/src/*.c \
    -O2 -Icommon -I"$OSM_DIR/libosmmesh/include" \
    -DW3_RAD=2 -DW3_TEX=768 -DW3_TERR=22 \
    -sUSE_SDL=2 -lwebsocket.js -sFULL_ES2 \
    -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=128MB -sSTACK_SIZE=4MB \
    -sEXIT_RUNTIME=0 \
    --preload-file "$VEC@/hameln.pmtiles" \
    --preload-file "$TER@/hameln_terrain.pmtiles" \
    -o flightbox/web/cc.js
echo "WASM -> flightbox/web/cc.js (+ cc.wasm, cc.data with Hameln PMTiles)"
