#!/bin/sh
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
BUILD=${OUTSHINE_BUILD_DIR:-$ROOT/build}
case "${1:-}" in
  --native-compile-flags) exec python3 test/scripts/cmake_context.py compile ;;
  --native-link-flags|--link-flags) exec python3 test/scripts/cmake_context.py link ;;
esac
mode=suite
sanitizer=
hardened=OFF
timeout=120
configure=0
scopes=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --library) mode=library; shift ;;
    --compile-db) mode=database; shift ;;
    --cases) mode=list; shift ;;
    --all) mode=all; shift ;;
    --sanitizer) sanitizer=$2; shift 2 ;;
    --harden) hardened=ON; shift ;;
    --timeout) timeout=$2; configure=1; shift 2 ;;
    --*) printf 'Use CMake/CTest options directly: unsupported legacy option %s\n' "$1" >&2; exit 2 ;;
    *) scopes="$scopes $1"; shift ;;
  esac
done
if [ -n "$sanitizer" ]; then BUILD="$BUILD/$sanitizer"; fi
if [ ! -f "$BUILD/CMakeCache.txt" ] || [ -n "$sanitizer" ] || [ "$hardened" = ON ] || [ "$configure" = 1 ]; then
  mkdir -p "$BUILD/.cmake/api/v1/query"
  : > "$BUILD/.cmake/api/v1/query/codemodel-v2"
  cmake -S "$ROOT" -B "$BUILD" -G Ninja -DBUILD_TESTING=ON \
    -DOUTSHINE_SANITIZER="$sanitizer" -DOUTSHINE_HARDENED="$hardened" \
    -DOUTSHINE_TEST_TIMEOUT="$timeout"
fi
case "$mode" in
  library) exec cmake --build "$BUILD" --target outshine --parallel ;;
  database) exit 0 ;;
  list) exec ctest --test-dir "$BUILD" --show-only ;;
  all)
    cmake --build "$BUILD" --target outshine_tests outshine --parallel
    exec ctest --test-dir "$BUILD" --no-tests=error --output-on-failure
    ;;
esac
if [ -z "$scopes" ]; then
  cmake --build "$BUILD" --target outshine_gate --parallel
  exec ctest --test-dir "$BUILD" -L world-gate --no-tests=error --output-on-failure
fi
for scope in $scopes; do
  target=$(printf 'test_%s' "${scope%/}" | tr / _)
  cmake --build "$BUILD" --target "$target" --parallel
  ctest --test-dir "$BUILD" -R "^${scope%/}(/|\$)" --no-tests=error --output-on-failure
done
