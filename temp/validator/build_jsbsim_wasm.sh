#!/usr/bin/env bash
# Build libJSBSim as a WASM static archive from the pinned, read-only JSBSim submodule, for linking
# into the WASM Command Center (make controlcenter). JSBSim source stays vanilla — the only adaptation
# is the emscripten/musl strerror_r shim in aircraft/fdm/em_compat.h, force-included here.
set -euo pipefail
SIM="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_DIR="${EMSDK_DIR:-$HOME/Git/emsdk}"
BUILD="$SIM/validator/build/jsbsim-wasm"
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
mkdir -p "$BUILD"
if [ ! -f "$BUILD/src/libJSBSim.a" ]; then
  emcmake cmake -S "$SIM/jsbsim" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_PYTHON_MODULE=OFF -DBUILD_DOCS=OFF \
    -DSYSTEM_EXPAT=OFF \
    -DCMAKE_CXX_FLAGS="-fexceptions -include $SIM/aircraft/fdm/em_compat.h" \
    -DCMAKE_C_FLAGS="-include $SIM/aircraft/fdm/em_compat.h" >/dev/null
fi
emmake make -C "$BUILD" -j"$(nproc)" libJSBSim
echo "[jsbsim-wasm] -> $BUILD/src/libJSBSim.a"
