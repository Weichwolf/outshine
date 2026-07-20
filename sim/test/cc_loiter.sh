#!/usr/bin/env bash
# Default watch demo: an aircraft circling over Hameln at 500 m / 2000 m radius, live in the WASM
# Command Center (localhost:8080). Brings up flightbox at the loiter origin, then flies the ring.
#
#   test/cc_loiter.sh [aircraft] [lat] [lon] [alt_m] [radius_m]   defaults: c172 Hameln 500 2000
set -euo pipefail
cd "$(dirname "$0")/.."
AC="${1:-c172}"; LAT="${2:-52.045}"; LON="${3:-9.385}"; ALT="${4:-500}"; RAD="${5:-2000}"
SHIM=/tmp/fb-libfbclock.so
[ -f "$SHIM" ] || gcc -O2 -fPIC -shared aircraft/msp_bridge/fbclock.c -o "$SHIM" -ldl

echo ">> flightbox at loiter origin $LAT,$LON"
podman rm -f fb-flightbox >/dev/null 2>&1 || true
podman network exists flightboxnet || podman network create flightboxnet >/dev/null
podman run -d --name fb-flightbox --network flightboxnet -p 8080:8080 \
    -e AIRCRAFT_ADDR=fb-aircraft -e ORIGIN_LAT="$LAT" -e ORIGIN_LON="$LON" \
    -e TILES_URL="http://localhost:8081" -e SIM_UTC=0 fb-flightbox >/dev/null
echo ">> $AC loitering ${RAD}m @ ${ALT}m — watch http://localhost:8080"

FB_TIME_SCALE="${FB_TIME_SCALE:-1}" LD_PRELOAD="$SHIM" \
    MOUNT_EEPROM="$PWD/aircraft/models/$AC/eeprom.bin" \
    python3 test/cc_loiter.py "$AC" "$LAT" "$LON" "$ALT" "$RAD"
