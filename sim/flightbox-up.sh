#!/usr/bin/env bash
# Bring up the ONE persistent flightbox + aircraft the way the architecture intends: the aircraft
# (JSBSim World + vanilla iNav) and the flightbox hub run continuously; command centers — the WASM CC
# (:8080), the headless TS CC/tests (:5766, MSP-proxy), and you watching — connect and leave at will,
# all sharing the one iNav link the hub multiplexes. Re-run to switch aircraft (recycles both).
#
#   flightbox-up.sh <c172|sgs233|f16>        FB_TIME_SCALE=<n> (default 1 = deterministic)
#
# Then: cc/dist/bin/cc.js watch   |  the browser at :8080  |  the test harness — all in parallel.
set -euo pipefail
cd "$(dirname "$0")"
AC="${1:?usage: flightbox-up.sh <c172|sgs233|f16>}"
N="${FB_TIME_SCALE:-1}"
NET=flightboxnet
[ -f "aircraft/models/$AC/eeprom.bin" ] || { echo "no eeprom for $AC — run aircraft/make-eeprom.sh $AC"; exit 1; }

read -r OLAT OLON OHDG < <(python3 - "$AC" <<'PY'
import sys, json; sys.path.insert(0, "test"); import mission
m = json.load(open(f"missions/{sys.argv[1]}.json"))
r = mission.runway(m["takeoff"]["airport"], m["takeoff"]["runway"])
print(r["lat"], r["lon"], r["heading_deg"])
PY
)
echo ">> $AC from $OLAT,$OLON hdg $OHDG  (time-scale ${N}x)"

podman network exists "$NET" || podman network create "$NET" >/dev/null
podman rm -f fb-aircraft fb-flightbox >/dev/null 2>&1 || true

# iNav SITL opens its eeprom READ-WRITE (it persists config) -> give it a fresh writable COPY so the
# committed eeprom.bin stays pristine. A ':ro' mount makes SITL fail '[EEPROM] Failed to create' and die.
WEEPROM="/tmp/fb-eeprom-$AC.bin"; cp "aircraft/models/$AC/eeprom.bin" "$WEEPROM"

# aircraft: JSBSim + vanilla iNav, spawned disarmed at the runway threshold
podman run -d --name fb-aircraft --network "$NET" -p 5761:5761 \
  -v "$WEEPROM:/app/models/$AC/eeprom.bin" \
  -v "$PWD/aircraft/models/$AC/profile.env:/app/models/$AC/profile.env:ro" \
  $([ -f "aircraft/models/$AC/$AC.xml" ] && echo "-v $PWD/aircraft/models/$AC/$AC.xml:/app/models/$AC/$AC.xml:ro") \
  -e AIRCRAFT="$AC" -e TILES_ADDR=fb-tiles:8081 -e WX_LIVE=0 -e WIND_SPEED=0 -e TURB=0 \
  -e FB_TIME_SCALE="$N" -e FLT_LOG_S="${FLT_LOG_S:-1}" \
  -e ORIGIN_LAT="$OLAT" -e ORIGIN_LON="$OLON" -e ORIGIN_HDG="$OHDG" fb-aircraft >/dev/null

# flightbox hub: one MSP link to the aircraft (:5762), fans telemetry out + forwards commands in;
# serves the WASM CC on :8080 and the headless-CC MSP proxy on :5766.
podman run -d --name fb-flightbox --network "$NET" -p 8080:8080 -p 5766:5766 \
  -e AIRCRAFT_ADDR=fb-aircraft -e ORIGIN_LAT="$OLAT" -e ORIGIN_LON="$OLON" \
  -e TILES_URL="http://localhost:8081" -e SIM_UTC=0 fb-flightbox >/dev/null

echo ">> up: aircraft + flightbox running."
echo "   WASM CC : http://localhost:8080"
echo "   CC proxy: localhost:5766   (cc/dist/bin/cc.js watch  |  test harness)"
