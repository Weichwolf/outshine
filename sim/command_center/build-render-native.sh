#!/usr/bin/env bash
# Build the headless offscreen renderer (EGL + GLES2 + osmmesh) used for visual
# evaluation of the command-center view without a browser. Renders the SAME
# world3d.h scene the WASM build shows. Output is raw RGB; convert with PIL.
#
#   ./build-render-native.sh
#   LIBGL_ALWAYS_SOFTWARE=1 ./render_native out.rgb <vec.pmtiles> <terr.pmtiles> \
#        [lat lon] [W H] [roll pitch yaw alt east north]
#   python3 -c "from PIL import Image;Image.frombytes('RGB',(W,H),open('out.rgb','rb').read()).save('out.png')"
set -euo pipefail
cd "$(dirname "$0")"
: "${OSM_DIR:=$HOME/Git/wasm-osm}"
LIB="$OSM_DIR/build-linux/libosmmesh/libosmmesh.a"
[ -f "$LIB" ] || { echo "missing $LIB — build libosmmesh (native) first"; exit 1; }
gcc -O2 -I../common -I"$OSM_DIR/libosmmesh/include" render_native.c "$LIB" \
    -o render_native -lEGL -lGLESv2 -lm
echo "built ./render_native"
