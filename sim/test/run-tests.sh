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

start_stack(){   # $1 = FDM_MODEL
  podman rm -f tf-aircraft tf-flightbox >/dev/null 2>&1 || true
  podman run -d --name tf-flightbox --network "$NET" -e AIRCRAFT_ADDR=tf-aircraft -p "$PORT":8080 "$IMG_F" >/dev/null || {
      echo "  FATAL: tf-flightbox did not start (port $PORT taken? set TEST_PORT=)"; exit 1; }
  podman run -d --name tf-aircraft  --network "$NET" -e FLIGHTBOX_ADDR=tf-flightbox -e FDM_MODEL="$1" -e TEST_MODE=1 "$IMG_A" >/dev/null || {
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
for i in "${!MODELS[@]}"; do
  M=${MODELS[$i]}
  echo; echo "############ E2E suite — FDM model $M (${NAMES[$i]}) ############"
  start_stack "$M"
  if ! HOST=127.0.0.1:$PORT python3 test/eval.py; then fail=1; fi
done

echo
if [ $fail -eq 0 ]; then echo "==================== ALL TESTS PASSED ===================="; else echo "==================== SOME TESTS FAILED ===================="; fi
exit $fail
