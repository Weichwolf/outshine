#!/usr/bin/env bash
# Unit tests + line-coverage report for the pure-logic modules.
#
# SCOPE, stated honestly. This covers code that CAN be covered by a unit test: pure maths,
# source descriptors and parsers. Deliberately NOT here, and why:
#   - tiles/cache.c                 is curl + stat + rename: I/O with no logic left in it once
#                                   the URL building (tilesrc.c) is factored out -> live route checks
#   - tiles/elev.c                  fetch + PNG decode glue. Its ONE piece of real logic was the
#                                   LRU eviction; that now lives in tiles/lru.h and IS covered.
#                                   The Terrarium decode itself is osmmesh's (vendored), and the
#                                   bilinear sample is tilemath.h -> both covered elsewhere.
#   - tiles/main.c                  is a `for(;;) accept()` server loop -> live route checks
#   - aircraft/terrain.c            polls a running service            -> live checks
#   - command_center/*              needs a GL context                 -> render_native + browser
#   - aircraft/xp_bridge.c          is the plant in a closed loop      -> test/eval.py (~7500 invariants)
#     (its PURE parts are being lifted out into fdm/* one at a time, and those ARE covered here)
#   - command_center/world3d.h      needs a GL context -> render_native + browser.
#     (same treatment: gfx/mat4.h and gfx/style.h are out and covered; sky/HUD/cache still inside)
#
# NAMED GAP, not rounded away: nothing here proves the wire between the modules. A correct
# atmosphere wired to the wrong dref, or a correct LRU whose payload array is off by one, passes
# every assertion in this file. That is what test/eval.py and the headless render are for --
# see test/verify.sh, which runs all of it together.
# Everything listed above is exercised elsewhere; nothing is simply unmeasured. Modules that ARE
# in scope must be at 100% — the run fails otherwise and prints the uncovered lines.
#
# Two gcov traps this script exists to avoid (both bit us):
#   - do NOT pass -r/--relative-only: our headers resolve to absolute paths, so it silently drops
#     them and then reports a meaningless "100% of 4 lines".
#   - gcov cannot merge across binaries. All tests therefore link into ONE binary; separate
#     binaries reported every file a given test didn't touch as 0%, so the "coverage" depended on
#     which report you read first.
# We parse gcov's own summary rather than counting lines ourselves — measuring the measurement
# is how you get fooled.
#
#   ./run.sh            run tests + print coverage
#   ./run.sh --html     also write lcov HTML (needs lcov)
set -uo pipefail
cd "$(dirname "$0")"
HERE=$(pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
fail=0

# modules under test (pure) + the test drivers
SRC="$ROOT/tiles/tilesrc.c $ROOT/tiles/route.c $ROOT/aircraft/fdm/atmosphere.c"
TESTS="$HERE/main.c $HERE/test_tilemath.c $HERE/test_tilesrc.c $HERE/test_route.c $HERE/test_atmosphere.c $HERE/test_mat4.c $HERE/test_style.c $HERE/test_lru.c $HERE/test_prefetch.c"

echo "== unit tests =="
( cd "$OUT" && gcc -O0 -g -Wall -Wextra --coverage -I"$HERE" -o unittests $TESTS $SRC -lm ) 2>"$OUT/cc.log" || {
    echo "  [BUILD FAIL]"; sed 's/^/      /' "$OUT/cc.log"; exit 1; }
( cd "$OUT" && ./unittests ) || fail=1

echo
echo "== line coverage (gcov's own numbers) =="
( cd "$OUT" && gcov ./unittests-*.gcda 2>/dev/null ) | awk -v root="$ROOT" '
  /^File / { path=$2; gsub(/[\x27"]/,"",path); file=path; next }
  /^Lines executed:/ {
      split($0,a,":"); split(a[2],b,"% of "); pct=b[1]+0; tot=b[2]+0;
      # skip the test drivers and their helpers: only production modules are reported
      if (file ~ /test\/unit\// || file !~ root) next;
      short=file; sub(root"/","",short);
      if (short in seen) next; seen[short]=1;
      if (pct >= 99.995) printf "  [100%%] %-26s (%d lines)\n", short, tot;
      else { printf "  [%5.1f%%] %-26s (%d lines)  <- NOT fully covered\n", pct, short, tot; bad=1 }
  }
  END { exit bad?1:0 }
' || { echo "  ^ a module in scope is below 100% — add tests or move it out of scope with a reason"; fail=1; }

if [ "${1:-}" = "--html" ] && command -v lcov >/dev/null; then
    lcov -c -d "$OUT" -o "$OUT/c.info" >/dev/null 2>&1
    genhtml "$OUT/c.info" -o "$HERE/coverage-html" >/dev/null 2>&1 && \
        echo "  HTML -> test/unit/coverage-html/index.html"
fi

echo
[ $fail -eq 0 ] && echo "== unit tests PASSED ==" || echo "== unit tests FAILED =="
exit $fail
