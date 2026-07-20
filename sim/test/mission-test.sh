#!/usr/bin/env bash
# Practice missions — the PPL(A) PRACTICAL exam. Flies all three aircraft through their mission files
# (missions/<ac>.json: waypoints + pass/abort conditions) with REAL iNav in the loop, accelerated
# FB_TIME_SCALE x via the clock shim. The shim is preloaded into THIS harness process too, so the CC
# shares the sim's scaled clock (time.monotonic) and the run stays deterministic while finishing N x
# faster than real time. Exit 0 = all three PASS. The theory counterpart is test/fdm-test.sh.
#
#   test/mission-test.sh            all three; FB_TIME_SCALE from env (default 6)
#   FB_TIME_SCALE=1 test/mission-test.sh   real-time (debugging)
set -uo pipefail
cd "$(dirname "$0")/.."                                  # -> sim/
N="${FB_TIME_SCALE:-6}"
SHIM=/tmp/fb-libfbclock.so
gcc -O2 -fPIC -shared aircraft/msp_bridge/fbclock.c -o "$SHIM" -ldl   # host build of the sim clock shim
fail=0
for ac in c172 sgs233 f16; do
    echo "== flying $ac (${N}x) =="
    if ! FB_TIME_SCALE="$N" LD_PRELOAD="$SHIM" MOUNT_EEPROM="$PWD/aircraft/models/$ac/eeprom.bin" \
         python3 test/e2e.py "$ac"; then fail=1; fi
done
echo; [ $fail -eq 0 ] && echo "== ALL THREE MISSIONS PASSED ==" || echo "== missions FAILED =="
exit $fail
