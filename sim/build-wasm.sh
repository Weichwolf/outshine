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

# The renderer is global camera-relative WGS84-ECEF (X-Plane/MSFS/DCS-style): the planet on the
# ellipsoid, the camera at the coordinate origin, per-tile double ECEF origins the frame subtracts.
# The old flat-ENU tangent plane (correct only ~20 km from a fixed home, horizon never dipped) was
# removed once ECEF was verified at Hameln AND a steep foreign origin (Aoraki). The worker carries
# the per-tile ECEF origin back as 3 doubles, so HEAPF64 is exported below.

# -Wall -Wextra: this build ran with NO warnings at all, and it cost real bugs. An unterminated
# comment silently swallowed an `else` (the compiler says "/*" within comment -- nobody was
# listening), and a later fix for that line then failed to match the mangled text and vanished
# without a trace. -Wno-unused-parameter only because the GL/SDL callbacks are full of them.
#
# -Wpedantic -Werror: a warning nobody has to act on is a warning nobody reads, and the two that
# stood here longest were both real -- a function and a variable that had outlived their only
# caller. MEASURED before turning this on: all 12 warnings were in command_center/, ZERO in the
# vendored geo/osmmesh (stb included). That is what makes -Werror affordable without -isystem
# islands or pragma exceptions -- our flags only ever gate our own code.
#
# The point is not tidiness. -Wswitch (already in -Wall) does nothing while a state is an int,
# but turns "one state too few" -- this renderer's most expensive bug class, e.g. PENDING vs
# ABSENT collapsed into one `if(n<=0)` -- into a build failure once those states are enums.
# The gate goes up first, then the states it is meant to guard.
emcc command_center/cc.c geo/osmmesh/src/*.c \
    -O2 -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Icommon -Igeo/osmmesh/include \
    -sUSE_SDL=2 -lwebsocket.js -sFULL_ES2 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 \
    -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=128MB -sSTACK_SIZE=4MB \
    -sEXIT_RUNTIME=0 \
    -o flightbox/web/cc.js
echo "WASM -> flightbox/web/cc.js (+ cc.wasm) — no bundled region; tiles come from fb-tiles"

# The tile worker: a SECOND wasm that runs osmmesh_fetch_tile + w3_chunk_build_ecef OFF the main thread,
# loaded with `new Worker` (tileworker.js glue). Built separately on purpose:
#   - it carries NO SDL/WebGL -- it never touches the GPU, it hands back a vertex array;
#   - ASYNCIFY lives HERE and not in cc.js, because only the worker blocks
#     (EMSCRIPTEN_FETCH_SYNCHRONOUS: a worker MAY wait, the main thread never may), so only it pays
#     the ~35 KB. Measured: tw.wasm is ~196 KB, NOT "osmmesh a second time" -- and how much cc.wasm
#     shrinks once osmmesh leaves the main module is a number to MEASURE when that lands, not now.
# MODULARIZE + ENVIRONMENT=worker so the glue can `new TileWorker()` inside the Worker scope.
emcc command_center/tileworker.c geo/osmmesh/src/*.c \
    -O2 -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter \
    -Icommon -Igeo/osmmesh/include -Icommand_center \
    -sFETCH -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sEXIT_RUNTIME=0 \
    -sMODULARIZE=1 -sEXPORT_NAME=TileWorker -sENVIRONMENT=worker \
    -sEXPORTED_RUNTIME_METHODS=ccall,HEAPF32,HEAPF64 \
    -o flightbox/web/tw.js
echo "WASM -> flightbox/web/tw.js (+ tw.wasm) — off-thread tile fetch/decode/mesh"
