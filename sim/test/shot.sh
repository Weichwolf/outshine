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
#   ./shot.sh out.png [osm|photo] [WxH] [timeout-seconds]
#
# The seconds are a TIMEOUT, not a sleep: shot.js waits for the streamer to report "chunks drawn,
# 0 pending" and shoots then. Exit 2 means it never got there — the picture is of an unfinished
# world and must not be read as a verdict on the renderer.
#
# Needs the stack running (../run-podman.sh) — it screenshots http://localhost:8080 like a user.
set -uo pipefail
cd "$(dirname "$0")"

OUT="${1:-/tmp/fb-shot.png}"
GROUND="${2:-osm}"
SIZE="${3:-1280x720}"
WAIT="${4:-90}"

command -v node >/dev/null || { echo "no node on PATH"; exit 1; }
node -e "require('playwright')" 2>/dev/null || { echo "playwright not resolvable by node"; exit 1; }

# Chrome comes from the playwright cache — see the toolchain notes in CLAUDE.md. Pinned rather
# than left to playwright, which asks for a revision it did not install. Overridable so this is
# not welded to one machine.
export FB_CHROME="${FB_CHROME:-$HOME/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome}"
[ -x "$FB_CHROME" ] || { echo "no chrome at $FB_CHROME — set FB_CHROME"; exit 1; }

curl -s -f --max-time 3 http://localhost:8080/config.js >/dev/null || {
    echo "nothing serving on :8080 — start the stack with sim/run-podman.sh"; exit 1; }

W="${SIZE%x*}"; H="${SIZE#*x}"
rm -f "$OUT"
node shot.js "$OUT" "$GROUND" "$W" "$H" "$WAIT"
rc=$?

[ -s "$OUT" ] || { echo "no screenshot produced"; exit 1; }
echo "$OUT  ($(stat -c%s "$OUT") bytes, ground=$GROUND, ${W}x${H})"
exit $rc
