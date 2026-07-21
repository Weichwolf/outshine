#!/usr/bin/env bash
# Bring up the ONE persistent sim container: JSBSim (World) + vanilla iNav (Pilot) + the flightbox hub
# (CC gateway) together. Command centers — the WASM CC (:8080), the headless TS CC / tests (:5766,
# MSP-proxy), and you watching — connect and leave at will, all sharing the one iNav link the hub
# multiplexes on loopback. Re-run to switch aircraft (recycles the container). Needs fb-tiles running.
#
#   flightbox-up.sh <c172|sgs233|f16>        FB_TIME_SCALE=<n> (default 1 = deterministic)
set -euo pipefail
cd "$(dirname "$0")"
AC="${1:?usage: flightbox-up.sh <c172|sgs233|f16>}"
N="${FB_TIME_SCALE:-1}"
NET=flightboxnet
[ -f "aircraft/models/$AC/eeprom.bin" ] || { echo "no eeprom for $AC — run: make eeprom AC=$AC"; exit 1; }

# takeoff-runway spawn origin via the native TS resolver (Python-free; MISSION overrides the default $AC.json)
read -r OLAT OLON OHDG < <(node cc/dist/bin/cc.js origin "${MISSION:-$AC}")
echo ">> $AC from $OLAT,$OLON hdg $OHDG  (time-scale ${N}x)"

# iNav SITL opens its eeprom READ-WRITE -> give it a fresh writable COPY so the committed eeprom.bin
# stays pristine (a ':ro' mount makes SITL die '[EEPROM] Failed to create').
WEEPROM="/tmp/fb-eeprom-$AC.bin"; cp "aircraft/models/$AC/eeprom.bin" "$WEEPROM"

podman network exists "$NET" || podman network create "$NET" >/dev/null
podman rm -f fb-flightbox >/dev/null 2>&1 || true

# Default: expose ONLY the HTTP API (:8080). All command centers — WASM (/ws) and headless TS CC (/msp) —
# talk to the hub over that one port. CC_DEBUG=1 also publishes iNav UART2 (:5761) for raw MSP poking.
PORTS="-p 8080:8080"; [ "${CC_DEBUG:-0}" = 1 ] && PORTS="$PORTS -p 5761:5761"
podman run -d --name fb-flightbox --network "$NET" $PORTS \
  -v "$WEEPROM:/app/models/$AC/eeprom.bin" \
  -v "$PWD/aircraft/models/$AC/profile.env:/app/models/$AC/profile.env:ro" \
  $([ -f "aircraft/models/$AC/$AC.xml" ] && echo "-v $PWD/aircraft/models/$AC/$AC.xml:/app/models/$AC/$AC.xml:ro") \
  -e AIRCRAFT="$AC" -e TILES_ADDR=fb-tiles:8081 -e TILES_URL="http://localhost:8081" \
  -e WX_LIVE=0 -e WIND_SPEED=0 -e TURB=0 -e SIM_UTC=0 \
  -e FB_TIME_SCALE="$N" -e FLT_LOG_S="${FLT_LOG_S:-1}" -e AIRCRAFT_ADDR=127.0.0.1 \
  -e ORIGIN_LAT="$OLAT" -e ORIGIN_LON="$OLON" -e ORIGIN_HDG="$OHDG" fb-flightbox >/dev/null

echo ">> up: JSBSim + iNav + hub in one container."
echo "   WASM CC : http://localhost:8080        (browser)"
echo "   headless: ws://localhost:8080/msp      (make watch  |  make tests)"
