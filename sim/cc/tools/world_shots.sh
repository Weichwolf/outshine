#!/usr/bin/env bash
# Worldwide F-16 loiter screenshot campaign — the render-hardening loop of the JSBSim-frontend goal.
# Serves flightbox/web/ STATICALLY per location (config.js hand-written — no flightbox hub container;
# only fb-tiles stays server-side) and probes the in-process WASM F-16 with Playwright.
#
#   cc/tools/world_shots.sh [secs-per-location]     shots -> /tmp/claude-*/cc-shots/<name>_*.png
set -euo pipefail
cd "$(dirname "$0")/../.."          # -> sim/
SECS="${1:-25}"
PORT=8090
TILES="${TILES_URL:-http://localhost:8081}"
NODEPATH="${CC_NODEPATH:-$HOME/.npm/_npx/e41f203b7505f1fb/node_modules}"
PROBE="$(mktemp -d)/cc_probe.cjs"; cp cc/tools/cc_probe.js "$PROBE"
WEB="$(mktemp -d)"; cp -r flightbox/web/. "$WEB/"

# name lat lon — deliberately diverse terrain: alps, himalaya, canyon, city, desert, fjord, atoll,
# high latitude, dead-flat plain, southern hemisphere.
LOCS='
grenchen      47.1800    7.4100
everest       27.9881   86.9250
grandcanyon   36.0544 -113.1540
manhattan     40.7580  -73.9855
sahara        23.0000    5.0000
geiranger     62.1049    7.0050
tokyo         35.6762  139.6503
atacama      -24.5000  -69.2500
tromso        69.6492   18.9553
bora          -16.5004 -151.7415
'

python3 -m http.server "$PORT" -d "$WEB" >/dev/null 2>&1 &
SRV=$!; trap 'kill $SRV 2>/dev/null' EXIT
sleep 0.5

echo "$LOCS" | while read -r NAME LAT LON; do
  [ -z "$NAME" ] && continue
  printf 'window.FB_ORIGIN_LAT=%s;window.FB_ORIGIN_LON=%s;window.FB_SIM_UTC=0;window.FB_TILES_URL='\''%s'\'';window.FB_GROUND_CLEAR=0;\n' \
    "$LAT" "$LON" "$TILES" > "$WEB/config.js"
  echo ">> $NAME ($LAT,$LON)"
  NODE_PATH="$NODEPATH" node "$PROBE" "$SECS" "world-$NAME" "http://localhost:$PORT/" \
    | grep -E '"booted"|"nanPackets"|world-' || true
done
echo ">> done — frames in /tmp/claude-*/cc-shots/world-*.png"
