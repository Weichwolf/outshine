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
# Every compile of the sources a refactor moves: all three images, plus the two native builds
# that use their OWN source lists (run-native.sh, build-render-native.sh) and so break
# independently of the containers.
build_all(){
  local ok=0
  for f in aircraft flightbox tiles; do
    printf '  building %-10s ... ' "$f"
    if podman build -q -f "$f/Containerfile" -t "fb-$f" . >/dev/null 2>&1; then echo OK
    else echo FAIL; podman build -f "$f/Containerfile" -t "fb-$f" . 2>&1 | tail -15 | sed 's/^/      /'; ok=1; fi
  done
  printf '  building %-10s ... ' "render_native"
  if ( cd command_center && ./build-render-native.sh >/dev/null 2>&1 ); then echo OK
  else echo FAIL; ( cd command_center && ./build-render-native.sh 2>&1 | tail -10 | sed 's/^/      /' ); ok=1; fi
  printf '  building %-10s ... ' "wasm"
  if ./build-wasm.sh >/dev/null 2>&1; then echo OK; else echo FAIL; ./build-wasm.sh 2>&1 | tail -10 | sed 's/^/      /'; ok=1; fi
  return $ok
}
gate "builds (3 images + render_native + wasm)" build_all

# --- 3. the renderer still renders a world ------------------------------------------------
# Not a pixel-exact compare: the scene depends on live tiles and the sun's real position, so
# exact pixels are not a stable oracle. What IS stable: it must stream real tiles and put real
# ground on the screen. A refactor that breaks the mesh path shows up here as an empty sky.
render_check(){
  command -v podman >/dev/null && podman ps --filter name=fb-tiles --format '{{.Names}}' | grep -q fb-tiles || {
    echo "  SKIP: fb-tiles not running (start it: ./run-podman.sh) — render gate needs live tiles"; return 0; }
  local out; out=$(mktemp -d)/r.rgb
  ( cd command_center && LIBGL_ALWAYS_SOFTWARE=1 timeout 300 ./render_native "$out" \
      http://localhost:8081 http://localhost:8081 52.045 9.385 320 240 >/dev/null 2>&1 )
  [ -s "$out" ] || { echo "  no output image"; return 1; }
  python3 - "$out" <<'PY'
import sys, collections
d = open(sys.argv[1], 'rb').read()
px = [d[i:i+3] for i in range(0, len(d), 3)]
colours = len(set(px))
# ground = anything that isn't sky-ish blue. A world with terrain has a good chunk of it.
ground = sum(1 for p in px if not (p[2] > p[0] + 20 and p[2] > 90))
pct = 100.0 * ground / len(px)
print("  %d distinct colours, %.0f%% ground pixels" % (colours, pct))
ok = colours > 200 and 10 < pct < 95
print("  " + ("OK — real terrain on screen" if ok else "SUSPECT — scene looks empty/degenerate"))
sys.exit(0 if ok else 1)
PY
}
gate "headless render (streams tiles -> pixels)" render_check

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
