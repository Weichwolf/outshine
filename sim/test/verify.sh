#!/usr/bin/env bash
# The refactor's safety net: everything that must still hold after moving code between files.
#
# WHY THIS EXISTS. Extracting fdm/ephemeris.{h,c} out of xp_bridge.c broke the aircraft
# Containerfile (the new fdm/ sources were never COPYd or compiled). The physics suite was green
# and the headless render was pixel-identical, so "no regression" looked proven — but nothing had
# rebuilt the image, and the running container was still the pre-extraction build. The gap wasn't
# that a check failed; it's that a check I never ran didn't exist in any list.
#
# A refactor is exactly the change that compiles fine in one build and breaks another. So the
# gates run TOGETHER or the claim isn't worth making.
#
#   ./verify.sh          all gates
#   ./verify.sh quick    skip the E2E physics suite (~minutes) — for tight extract/check loops
#
# Exit 0 only if every gate passes. Anything else means "no regression" is unproven.
set -uo pipefail
cd "$(dirname "$0")/.."            # -> sim/
QUICK="${1:-}"
fail=0
declare -a RESULT

gate(){   # gate <name> <cmd...>
  local name="$1"; shift
  printf '\n\033[1m== %s ==\033[0m\n' "$name"
  if "$@"; then RESULT+=("  PASS  $name"); else RESULT+=("  FAIL  $name"); fail=1; fi
}

# --- 1. pure logic: unit tests + 100% gate on modules in scope -----------------------------
gate "unit tests + coverage" test/unit/run.sh

# --- 2. builds. The gate the ephemeris extraction walked straight through. -----------------
# Every compile of the sources a refactor moves: all three images plus the WASM.
build_all(){
  local ok=0
  for f in aircraft flightbox tiles; do
    printf '  building %-10s ... ' "$f"
    if podman build -q -f "$f/Containerfile" -t "fb-$f" . >/dev/null 2>&1; then echo OK
    else echo FAIL; podman build -f "$f/Containerfile" -t "fb-$f" . 2>&1 | tail -15 | sed 's/^/      /'; ok=1; fi
  done
  printf '  building %-10s ... ' "wasm"
  if ./build-wasm.sh >/dev/null 2>&1; then echo OK; else echo FAIL; ./build-wasm.sh 2>&1 | tail -10 | sed 's/^/      /'; ok=1; fi
  return $ok
}
gate "builds (3 images + wasm)" build_all

# --- 3. the renderer still renders a world -------------------------------------------------
# Screenshots the REAL command center in a headless browser: same WASM, same JS, same HTML the
# user gets. This used to be render_native.c, a SECOND renderer written in C that re-created the
# camera/telemetry/scene setup cc.c already does. The duplication was not theoretical -- the two
# drifted: render_native compiled W3_TERR=24/W3_FARTEX=256 while the browser shipped 22/512, so
# the thing calling itself "the renderer's regression check" checked a scene nobody ran.
# Both ground sources are shot, which render_native never covered.
render_check(){
  curl -s -f --max-time 3 http://localhost:8080/config.js >/dev/null 2>&1 || {
    echo "  SKIP: stack not running (./run-podman.sh) — this gate screenshots the live app"; return 0; }
  local rc=0 d; d=$(mktemp -d)
  # 90 is a TIMEOUT, not a sleep: shot.sh returns as soon as the streamer reports "0 pending"
  # (~10 s warm), and only spends the 90 if the tiles genuinely are not coming. Its exit 2 --
  # "shot an unfinished world" -- must fail the gate rather than hand pngstat a half-empty sky
  # and let it pass judgement on the renderer for it.
  for g in osm photo; do
    if test/shot.sh "$d/$g.png" "$g" 800x600 90 >/dev/null 2>&1; then
      python3 test/pngstat.py "$d/$g.png" "$g" || rc=1
    else
      echo "  $g: no screenshot, or the world never finished streaming"; rc=1
    fi
  done
  rm -rf "$d"; return $rc
}
gate "headless render (real browser, osm + photo)" render_check

# --- 4. the physics still flies: ~7500 closed-loop invariants across 4 airframes ------------
if [ "$QUICK" = "quick" ]; then
  RESULT+=("  SKIP  E2E physics suite (quick mode)")
else
  gate "E2E physics suite (4 FDM models)" test/run-tests.sh
fi

printf '\n\033[1m===== verify summary =====\033[0m\n'
printf '%s\n' "${RESULT[@]}"
[ $fail -eq 0 ] && echo "ALL GATES PASSED — 'no regression' is measured, not claimed." \
                || echo "SOME GATES FAILED — do not claim 'no regression'."
exit $fail
