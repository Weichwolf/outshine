#!/usr/bin/env bash
# Build libJSBSim host-native (static, installed with headers) from the pinned, read-only JSBSim
# submodule (sim/vendor/jsbsim), for linking into the native WebGPU oracle (gpu_native). Out-of-source,
# idempotent — skips the build if the archive already exists.
set -euo pipefail
SIM="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$SIM/build"
JH="$BUILD/jsbsim-host"
mkdir -p "$BUILD"
if [ ! -f "$JH/lib/libJSBSim.a" ]; then
  echo "[jsbsim-host] building libJSBSim from sim/vendor/jsbsim submodule (~min)…"
  cmake -S "$SIM/vendor/jsbsim" -B "$BUILD/jsb" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_PYTHON_MODULE=OFF -DCMAKE_INSTALL_PREFIX="$JH" >/dev/null
  cmake --build "$BUILD/jsb" -j"$(nproc)" >/dev/null
  cmake --install "$BUILD/jsb" >/dev/null
fi
echo "[jsbsim-host] -> $JH/lib/libJSBSim.a"
