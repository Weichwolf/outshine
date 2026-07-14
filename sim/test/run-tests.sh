#!/usr/bin/env bash
# FlightBox test suite: builds + runs BOTH podman containers (test mode) and runs
# the end-to-end evaluation across several FDM models (realistic parameters).
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

start_stack(){   # $1 = FDM_MODEL
  podman rm -f tf-aircraft tf-flightbox >/dev/null 2>&1 || true
  podman run -d --name tf-flightbox --network "$NET" -e AIRCRAFT_ADDR=tf-aircraft -p 8080:8080 "$IMG_F" >/dev/null
  podman run -d --name tf-aircraft  --network "$NET" -e FLIGHTBOX_ADDR=tf-flightbox -e FDM_MODEL="$1" -e TEST_MODE=1 "$IMG_A" >/dev/null
  sleep 3
}

MODELS=(0 1 2 3)
NAMES=(ZOHD-Dart Sonicmodell-AR-Wing Skywalker-X8 Skywalker-X8-heavy)
for i in "${!MODELS[@]}"; do
  M=${MODELS[$i]}
  echo; echo "############ E2E suite — FDM model $M (${NAMES[$i]}) ############"
  start_stack "$M"
  if ! HOST=127.0.0.1:8080 python3 test/eval.py; then fail=1; fi
done

echo
if [ $fail -eq 0 ]; then echo "==================== ALL TESTS PASSED ===================="; else echo "==================== SOME TESTS FAILED ===================="; fi
exit $fail
