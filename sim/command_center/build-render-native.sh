#!/usr/bin/env bash
# Build the headless offscreen renderer (EGL + GLES2 + osmmesh) used for visual evaluation of
# the command-center view without a browser. Renders the SAME world3d.h scene the WASM build
# shows, so it doubles as the renderer's regression check. Output is raw RGB.
#
# Self-contained: compiles the VENDORED osmmesh sources under sim/geo/ — no external checkout.
#
#   ./build-render-native.sh
#   ../geo/fetch-data.sh                      # ensure the PMTiles cache is populated
#   LIBGL_ALWAYS_SOFTWARE=1 ./render_native out.rgb \
#        ../geo/data/hameln.pmtiles ../geo/data/hameln_terrain.pmtiles \
#        [lat lon] [W H] [roll pitch yaw alt east north]
#   python3 -c "from PIL import Image;Image.frombytes('RGB',(W,H),open('out.rgb','rb').read()).save('out.png')"
set -euo pipefail
cd "$(dirname "$0")"
gcc -O2 -I../common -I../geo/osmmesh/include \
    render_native.c ../geo/osmmesh/src/*.c \
    -o render_native -lEGL -lGLESv2 -lm
echo "built ./render_native  (vendored osmmesh; PMTiles under ../geo/data/)"
