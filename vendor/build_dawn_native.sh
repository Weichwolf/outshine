#!/bin/bash
# Build native Dawn (the C/C++ WebGPU implementation) src/render/ links against.
# Clone is fetched fresh (not vendored in git, see .gitignore); rerun after `rm -rf vendor/dawn`
# to update the pin.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

TAG=0bc38adde72b79013536f8ce354b639ae19ae195   # Dawn v20260720.160313

if [ ! -d dawn/.git ]; then
  echo "cloning Dawn @ $TAG ..."
  mkdir -p dawn && cd dawn
  git init -q
  git remote add origin https://dawn.googlesource.com/dawn
  git fetch --depth 1 origin "$TAG"
  git checkout -q FETCH_HEAD
  cd ..
fi

# Dawn's CMakeLists force-enables X11 on any non-Android/Emscripten UNIX build (no override option) —
# a COMPILE-time dep only (offscreen native never opens a window/surface). libx11-xcb-dev ships just
# that header; if it's not installed and there's no root to apt-get it, vendor a copy of the header
# ourselves (no root needed: apt-get download + dpkg-deb -x), cached so this only runs once.
XCOMPAT="$PWD/.compat-headers"
if [ ! -f /usr/include/X11/Xlib-xcb.h ] && [ ! -f "$XCOMPAT/X11/Xlib-xcb.h" ]; then
  echo "system libx11-xcb-dev missing (Xlib-xcb.h) -> vendoring the header, no root needed"
  tmp=$(mktemp -d)
  ( cd "$tmp" && apt-get download libx11-xcb-dev && dpkg-deb -x ./libx11-xcb-dev_*.deb x )
  mkdir -p "$XCOMPAT"
  cp -r "$tmp/x/usr/include/X11" "$XCOMPAT/"
  rm -rf "$tmp"
fi

mkdir -p dawn/out
cmake -S dawn -B dawn/out -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDAWN_FETCH_DEPENDENCIES=ON \
  -DDAWN_BUILD_SAMPLES=OFF \
  -DDAWN_BUILD_TESTS=OFF \
  -DDAWN_BUILD_NODE_BINDINGS=OFF \
  -DTINT_BUILD_TESTS=OFF \
  -DTINT_BUILD_CMD_TOOLS=OFF \
  -DCMAKE_CXX_FLAGS="-I$XCOMPAT" -DCMAKE_C_FLAGS="-I$XCOMPAT" \
  -DDAWN_SUPPORTS_CXX_MODULES=OFF   # GCC+Ninja C++20-modules dyndep hit an internal ninja assert on
                                    # incremental rebuild; we only #include webgpu_cpp.h, never `import`

# webgpu_dawn: the monolithic native C-API lib (dawn::webgpu_dawn); dawncpp_headers: generates
# webgpu/webgpu_cpp.h.
cmake --build dawn/out --target webgpu_dawn dawncpp_headers -j"$(nproc)"

echo "Dawn native -> vendor/dawn/out (libwebgpu_dawn.a + gen/include/webgpu/webgpu_cpp.h)"
