#!/usr/bin/env bash
# Reproducible host-native build of the fast mission validator: libJSBSim (from the pinned jsbsim submodule)
# + the extracted iNav control core + validator.c + the JSBSim adapter. Idempotent — skips libJSBSim if built.
set -euo pipefail
SIM="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$SIM/validator/build"           # gitignored host artifacts
JH="$BUILD/jsbsim-host"
mkdir -p "$BUILD"

# 1) libJSBSim host-native from the read-only submodule (out-of-source), once
if [ ! -f "$JH/lib/libJSBSim.a" ]; then
  echo "[validator] building libJSBSim from sim/jsbsim submodule (~min)…"
  cmake -S "$SIM/jsbsim" -B "$BUILD/jsb" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_PYTHON_MODULE=OFF -DCMAKE_INSTALL_PREFIX="$JH" >/dev/null
  cmake --build "$BUILD/jsb" -j"$(nproc)" >/dev/null
  cmake --install "$BUILD/jsb" >/dev/null
fi

# 2) (re)extract iNav's numeric control core from the pinned inav-src submodule
node "$SIM/validator/extract_inav.mjs"

# 3) compile: validator.c (C) + jsbsim_adapter.cpp (C++) → link libJSBSim
gcc -O2 -std=c11 -Wall -I"$SIM/aircraft/fdm" -I"$SIM/validator" \
    -c "$SIM/validator/validator.c" -o "$BUILD/validator.o"
g++ -O2 -std=c++17 -I"$SIM/aircraft/fdm" -I"$JH/include/JSBSim" \
    -c "$SIM/aircraft/fdm/jsbsim_adapter.cpp" -o "$BUILD/adapter.o"
g++ "$BUILD/validator.o" "$BUILD/adapter.o" "$JH/lib/libJSBSim.a" -lm -lpthread \
    -o "$SIM/validator/validator"
echo "[validator] built -> sim/validator/validator"
