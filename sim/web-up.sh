#!/usr/bin/env bash
# Bring up the lean fb-web container: just the HTTP/WS hub (fbserver) serving the browser Command
# Center + proxying MSP -- no JSBSim/iNav in here, those live in their own container(s) now.
# Web assets are a live MOUNT (sim/flightbox/web), so `make webgpu`/`make controlcenter` rebuilds
# show up on refresh, no image rebuild needed. Needs fb-tiles running (terrain/imagery).
#
#   web-up.sh                    AIRCRAFT_ADDR=<host> (default 127.0.0.1, MSP peer)
set -euo pipefail
cd "$(dirname "$0")"
NET=flightboxnet

podman build -f flightbox/Containerfile.web -t fb-web . >/dev/null

podman network exists "$NET" || podman network create "$NET" >/dev/null
podman rm -f fb-web >/dev/null 2>&1 || true

podman run -d --name fb-web --network "$NET" -p 8080:8080 \
  -v "$PWD/flightbox/web:/app/flightbox/web:ro" \
  -e AIRCRAFT_ADDR="${AIRCRAFT_ADDR:-127.0.0.1}" -e TILES_URL="http://localhost:8081" \
  -e ORIGIN_LAT="${ORIGIN_LAT:-47.179846}" -e ORIGIN_LON="${ORIGIN_LON:-7.411427}" \
  -e SIM_UTC="${SIM_UTC:-0}" fb-web >/dev/null

echo ">> up: fb-web (browser CC host) on :8080."
echo "   WASM CC : http://localhost:8080        (browser)"
echo "   headless: ws://localhost:8080/msp      (make watch  |  make tests)"
