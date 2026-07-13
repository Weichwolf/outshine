#!/usr/bin/env bash
# Run both parts natively (no containers) for quick iteration. UDP over loopback.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
gcc -O2 -Wall -Icommon aircraft/aircraft.c  -o build/aircraft  -lm
gcc -O2 -Wall -Icommon flightbox/server.c   -o build/flightbox
[ -f flightbox/web/cc.wasm ] || echo "WARN: flightbox/web/cc.wasm missing — run ./build-wasm.sh"

./build/aircraft & AC=$!
( cd flightbox && exec ../build/flightbox ) & FB=$!
trap 'kill $AC $FB 2>/dev/null || true' INT TERM EXIT
echo "Open http://localhost:8080  (Ctrl-C to stop)"
wait
