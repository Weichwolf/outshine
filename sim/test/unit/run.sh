#!/usr/bin/env bash
# Unit tests + line-coverage report for the pure-logic modules.
#
# Scope, honestly: this covers code that CAN be covered — pure maths and parsing. GL shaders
# need a context and the service loops are `for(;;) accept()`; those are exercised end-to-end by
# sim/test/eval.py, not here. Modules below 100% print their uncovered lines rather than having
# the number quietly rounded up.
#
# Note on gcov: do NOT pass -r/--relative-only. Our headers resolve to absolute paths, so it
# silently drops them and then reports a meaningless "100% of 4 lines". We parse gcov's own
# summary instead of counting lines ourselves — measuring the measurement is how you get fooled.
#
#   ./run.sh            run tests + print coverage
#   ./run.sh --html     also write lcov HTML (needs lcov)
set -uo pipefail
cd "$(dirname "$0")"
HERE=$(pwd)
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
fail=0

echo "== unit tests =="
for t in test_*.c; do
    ( cd "$OUT" && gcc -O0 -g -Wall -Wextra --coverage -o "${t%.c}" "$HERE/$t" -lm ) 2>"$OUT/cc.log" || {
        echo "  [BUILD FAIL] $t"; sed 's/^/      /' "$OUT/cc.log"; fail=1; continue; }
    ( cd "$OUT" && "./${t%.c}" ) || fail=1
done

echo
echo "== line coverage (gcov's own numbers) =="
( cd "$OUT" && gcov ./*.gcda 2>/dev/null ) | awk -v root="$(cd "$HERE/../.." && pwd)" '
  /^File / {
      path=$2; gsub(/['\''"]/,"",path); file=path; next
  }
  /^Lines executed:/ {
      split($0,a,":"); split(a[2],b,"% of "); pct=b[1]+0; tot=b[2]+0;
      # only report our own sources, not the test drivers or system headers
      if (file ~ /\/test_/ || file !~ root) next;
      short=file; sub(root"/","",short);
      if (short in seen) next;                 # gcov can emit a file once per object; report once
      seen[short]=1;
      if (pct >= 99.995) printf "  [100%%] %-28s (%d lines)\n", short, tot;
      else { printf "  [%5.1f%%] %-28s (%d lines)  <- NOT fully covered\n", pct, short, tot; bad=1 }
  }
  END { exit bad?1:0 }
' || { echo "  ^ a module is below 100% — add tests or justify the gap"; fail=1; }

if [ "${1:-}" = "--html" ] && command -v lcov >/dev/null; then
    lcov -c -d "$OUT" -o "$OUT/c.info" >/dev/null 2>&1
    genhtml "$OUT/c.info" -o "$HERE/coverage-html" >/dev/null 2>&1 && \
        echo "  HTML -> test/unit/coverage-html/index.html"
fi

echo
[ $fail -eq 0 ] && echo "== unit tests PASSED ==" || echo "== unit tests FAILED =="
exit $fail
