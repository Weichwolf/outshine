#!/usr/bin/env bash
# Bring up the lean fb-sim web host: the HTTP server for the browser Command Center + runtime env
# config. web/ is a live MOUNT (sim/web), so a `make wasm` rebuild shows up on refresh —
# no image rebuild. Needs fb-tiles running (terrain/imagery).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p shots logs runs   # the standpoint log (L in the browser) and every run log land here, on the HOST
NET=flightboxnet

podman build -f Dockerfile -t fb-sim . >/dev/null

podman network exists "$NET" || podman network create "$NET" >/dev/null
podman rm -f fb-sim >/dev/null 2>&1 || true

podman run -d --name fb-sim --network "$NET" -p 8080:8080 \
  -v "$PWD/web:/app/web:ro" \
  -v "$PWD/shots:/app/shots" \
  -v "$PWD/logs:/app/logs" \
  -v "$PWD/runs:/app/runs" \
  -e TILES_URL="${TILES_URL:-http://localhost:8081}" \
  -e ORIGIN_LAT="${ORIGIN_LAT:-47.179846}" -e ORIGIN_LON="${ORIGIN_LON:-7.411427}" \
  -e SIM_UTC="${SIM_UTC:-0}" \
  -e OUTSHINE_MOD="${OUTSHINE_MOD:-demo}" -e OUTSHINE_SCENE="${OUTSHINE_SCENE:-walk}" fb-sim >/dev/null

echo ">> up: fb-sim (browser CC host) on :8080  ->  http://localhost:8080"
