#!/usr/bin/env bash
# Bring up a LIVE mission the WASM Command Center (localhost:8080) can watch, at the mission's own
# airport: restart fb-flightbox with ORIGIN = the takeoff runway (so its ENU telemetry origin matches
# what the aircraft flies), then fly the mission in HOLD mode (keeps flying/loitering past the last WP,
# container stays up) so Playwright agents have a persistent telemetry+render target.
#
#   test/cc-mission.sh <c172|sgs233|f16> [sim_seconds]
#
# Leaves fb-aircraft + fb-flightbox running. Re-run for the next aircraft; it recycles both.
set -euo pipefail
cd "$(dirname "$0")/.."
AC="${1:?usage: cc-mission.sh <c172|sgs233|f16> [secs]}"
SECS="${2:-600}"
SHIM=/tmp/fb-libfbclock.so
[ -f "$SHIM" ] || gcc -O2 -fPIC -shared aircraft/msp_bridge/fbclock.c -o "$SHIM" -ldl

# takeoff origin (lat/lon) from the mission's runway, via the OurAirports DB in mission.py
read -r OLAT OLON < <(python3 - "$AC" <<'PY'
import sys, json
sys.path.insert(0, "test"); import mission
m = json.load(open(f"missions/{sys.argv[1]}.json"))
r = mission.runway(m["takeoff"]["airport"], m["takeoff"]["runway"])
print(r["lat"], r["lon"])
PY
)
echo ">> $AC live from $OLAT,$OLON — restarting flightbox at that origin"

# flightbox must sit on flightboxnet with the mission origin so ENU x/y is right and it can reach fb-aircraft:5762
podman rm -f fb-flightbox >/dev/null 2>&1 || true
podman network exists flightboxnet || podman network create flightboxnet >/dev/null
podman run -d --name fb-flightbox --network flightboxnet -p 8080:8080 \
    -e AIRCRAFT_ADDR=fb-aircraft -e ORIGIN_LAT="$OLAT" -e ORIGIN_LON="$OLON" \
    -e TILES_URL="http://localhost:8081" -e SIM_UTC=0 fb-flightbox >/dev/null
echo ">> flightbox up on :8080 (origin $OLAT,$OLON). Flying $AC (HOLD ${SECS}s)…"

# The F-16 is the marginal airframe: at 2x sim-speed the added flightbox telemetry poll (7 MSP reqs/tick
# on UART3) overloads the SITL scheduler enough to tip it into a NAV dive that the headless run (no CC
# attached) never sees. Real-time gives the scheduler the headroom, so the live CC run matches headless.
DEF_SCALE=2; [ "$AC" = f16 ] && DEF_SCALE=1
FB_HOLD=1 FB_TIME_SCALE="${FB_TIME_SCALE:-$DEF_SCALE}" LD_PRELOAD="$SHIM" \
    MOUNT_EEPROM="$PWD/aircraft/models/$AC/eeprom.bin" python3 test/e2e.py "$AC" "$SECS"
