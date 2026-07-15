#!/usr/bin/env bash
# Screenshot the command center — the REAL one, as shipped.
#
# This replaces render_native.c, which was a second renderer written in C that re-created the
# camera, telemetry and scene setup cc.c already does. That duplication was not theoretical: the
# two builds drifted apart (render_native compiled W3_TERR=24 / W3_FARTEX=256 while the browser
# shipped 22 / 512), so the thing calling itself "the renderer's regression check" was checking a
# scene nobody ran. A headless browser loads the same WASM, the same JS and the same HTML the
# user gets, so there is nothing left to drift.
#
#   ./shot.sh out.png [osm|photo] [WxH] [seconds]
#
# Needs the stack running (../run-podman.sh) — it screenshots http://localhost:8080 like a user.
set -uo pipefail
cd "$(dirname "$0")"

OUT="${1:-/tmp/fb-shot.png}"
GROUND="${2:-osm}"
SIZE="${3:-1280x720}"
WAIT="${4:-25}"

# Chrome comes from the playwright cache — see the toolchain notes in CLAUDE.md. Overridable so
# this is not welded to one machine.
CHROME="${FB_CHROME:-$HOME/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome}"
[ -x "$CHROME" ] || { echo "no chrome at $CHROME — set FB_CHROME"; exit 1; }

curl -s -f --max-time 3 http://localhost:8080/config.js >/dev/null || {
    echo "nothing serving on :8080 — start the stack with sim/run-podman.sh"; exit 1; }

URL="http://localhost:8080/?ground=$GROUND"
W="${SIZE%x*}"; H="${SIZE#*x}"

# --use-angle=swiftshader: software GL, so this works headless with no GPU. NOT --use-gl=
# swiftshader: this Chrome answers that with "Requested GL implementation (gl=none,angle=none)
# not found", renders a blank white canvas, and still exits 0 with a screenshot -- a green-looking
# check of an empty page.
# --virtual-time-budget: chrome fast-forwards its clock and only shoots once the page goes idle,
#   which is what gives the tile streaming time to actually land. Wall-clock sleeps would race.
rm -f "$OUT"
timeout $((WAIT + 40)) "$CHROME" --headless --no-sandbox --disable-gpu-sandbox \
    --use-angle=swiftshader --enable-unsafe-swiftshader \
    --screenshot="$OUT" --window-size="$W,$H" \
    --virtual-time-budget=$((WAIT * 1000)) \
    "$URL" >/dev/null 2>&1

[ -s "$OUT" ] || { echo "no screenshot produced"; exit 1; }
echo "$OUT  ($(stat -c%s "$OUT") bytes, ground=$GROUND, ${W}x${H})"
