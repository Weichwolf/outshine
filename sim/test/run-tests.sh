#!/usr/bin/env bash
# FlightBox test suite: builds + runs BOTH podman containers (test mode) and runs
# the end-to-end evaluation across several FDM models (realistic parameters).
#
# Runs on TEST_PORT (default 8099), NOT 8080 — so it can run alongside a live run-podman.sh stack
# without silently measuring it instead. See start_stack() for the incident this prevents.
set -uo pipefail
cd "$(dirname "$0")/.."            # -> sim/
NET=flightboxnet-test
IMG_A=fb-aircraft IMG_F=fb-flightbox
fail=0

echo "== build WASM + container images =="
[ -f flightbox/web/cc.wasm ] || ./build-wasm.sh
podman build -q -f aircraft/Containerfile  -t "$IMG_A" . >/dev/null || { echo "aircraft build FAIL"; exit 1; }
podman build -q -f flightbox/Containerfile -t "$IMG_F" . >/dev/null || { echo "flightbox build FAIL"; exit 1; }
podman network exists "$NET" || podman network create "$NET" >/dev/null

teardown(){ podman rm -f tf-aircraft tf-flightbox >/dev/null 2>&1 || true; }
trap teardown EXIT
teardown

# The test stack gets its OWN port. It used to publish 8080 — the same port the live stack from
# run-podman.sh holds — and `podman run` output went to /dev/null with no exit check, so on a
# machine with the sim running the container silently never started and eval.py happily connected
# to the LIVE flightbox instead. Every "FDM model" then measured the same running aircraft, and
# the suite reported four models it had never started. A red result from a test that measured the
# wrong process is worse than no test. Hence: own port, and every failure is fatal and loud.
PORT="${TEST_PORT:-8099}"

# TEST WEATHER — two presets, each COHERENT with the atmosphere the sim itself believes in.
# xp_bridge.c derives turbulence from the gust spread:  turb = min(1, 0.08*wsp + 0.11*(gust-wsp)).
# The old fixed pair (WIND_SPEED=3.5, TURB=1.0) is nowhere on that curve: turb=1.0 at 3.5 m/s
# implies a gust spread of (1.0-0.08*3.5)/0.11 = 6.5 m/s, i.e. gusts of 10 m/s over a 3.5 m/s
# wind — a gust factor of 2.9, where the real boundary layer gives 1.3-1.8. We were validating
# the flight model against an atmosphere that cannot occur, which is a strange yardstick for
# "as close as possible to the real aircraft". Both presets below sit ON the curve:
#   calm  2.5 m/s wind, gusts 3.5 (factor 1.4) -> turb 0.31  -- a still morning
#   rough 6.5 m/s wind, gusts 11.0 (factor 1.7) -> turb 1.00  -- a breezy afternoon, turb at its cap
# BOTH must pass: turbulence hides some faults and manufactures others, so a gate tuned on one
# proves nothing about the other. Override with TEST_WEATHERS="calm" for a quick single pass.
declare -A WX_WIND=( [calm]=2.5 [rough]=6.5 )
declare -A WX_TURB=( [calm]=0.3 [rough]=1.0 )
WEATHERS=(${TEST_WEATHERS:-calm rough})

start_stack(){   # $1 = FDM_MODEL  $2 = weather preset name
  podman rm -f tf-aircraft tf-flightbox >/dev/null 2>&1 || true
  podman run -d --name tf-flightbox --network "$NET" -e AIRCRAFT_ADDR=tf-aircraft -p "$PORT":8080 "$IMG_F" >/dev/null || {
      echo "  FATAL: tf-flightbox did not start (port $PORT taken? set TEST_PORT=)"; exit 1; }
  # FIXED WEATHER. This used to pass TEST_MODE=1 -- which NOTHING reads (grep it: the only hits
  # are vendored iNav USB constants). So the suite looked controlled and wasn't: g_wx_live
  # defaults to 1, every container start fetched the REAL wind over Hameln from Open-Meteo, and
  # the result depended on the actual weather at the moment you ran it. A calm afternoon passed;
  # a gusty one tripped marginal thresholds. You cannot prove "no regression" with a test whose
  # input is the sky. WX_LIVE=0 + explicit wind/turbulence makes a run comparable to another run.
  podman run -d --name tf-aircraft  --network "$NET" -e FLIGHTBOX_ADDR=tf-flightbox -e FDM_MODEL="$1" \
      -e WX_LIVE=0 \
      -e WIND_SPEED="${TEST_WIND_SPEED:-${WX_WIND[$2]}}" -e WIND_DIR="${TEST_WIND_DIR:-240}" \
      -e TURB="${TEST_TURB:-${WX_TURB[$2]}}" -e THERMAL="${TEST_THERMAL:-0}" \
      "$IMG_A" >/dev/null || {
      echo "  FATAL: tf-aircraft did not start"; exit 1; }
  sleep 3
  # Prove we are talking to OUR stack, not something else that happens to hold the port.
  for i in $(seq 1 20); do
      curl -s -f --max-time 2 "http://127.0.0.1:$PORT/config.js" >/dev/null 2>&1 && break
      [ "$i" = 20 ] && { echo "  FATAL: test flightbox not serving on $PORT after 20s"; \
                         podman logs --tail 5 tf-flightbox 2>&1 | sed 's/^/      /'; exit 1; }
      sleep 1
  done
  podman ps --filter name=tf-aircraft --format '{{.Names}}' | grep -q tf-aircraft || {
      echo "  FATAL: tf-aircraft exited immediately"; podman logs --tail 5 tf-aircraft 2>&1 | sed 's/^/      /'; exit 1; }
}

MODELS=(0 1 2 3)
NAMES=(ZOHD-Dart Sonicmodell-AR-Wing Skywalker-X8 Skywalker-X8-heavy)
for W in "${WEATHERS[@]}"; do
  for i in "${!MODELS[@]}"; do
    M=${MODELS[$i]}
    echo; echo "############ E2E suite — FDM model $M (${NAMES[$i]}) — weather $W (${WX_WIND[$W]} m/s, turb ${WX_TURB[$W]}) ############"
    start_stack "$M" "$W"
    if ! HOST=127.0.0.1:$PORT python3 test/eval.py; then fail=1; echo "  ^^ FAILED: model $M / weather $W"; fi
  done
done

echo
if [ $fail -eq 0 ]; then echo "==================== ALL TESTS PASSED ===================="; else echo "==================== SOME TESTS FAILED ===================="; fi
exit $fail
