#!/usr/bin/env bash
# FDM fixture suite — the PPL(A) THEORY exam. Deterministic flight-dynamics checks (environment +
# flight-state + control signal -> expected result) run against the SAME static libJSBSim the sim flies,
# with the real aircraft/models mounted. No iNav, no network, no rendering — pure physics, same numbers
# every run. The practical counterpart is the missions flown by test/e2e.py.
#
#   test/fdm-test.sh        build + run; exit 0 = all green
set -euo pipefail
cd "$(dirname "$0")/.."                       # -> sim/
podman image exists fb-aircraft || { echo "build fb-aircraft first (podman build -f aircraft/Containerfile -t fb-aircraft .)"; exit 1; }
exec podman run --rm -v "$PWD":/w:ro -w /w fb-aircraft sh -euc '
  g++ -O2 -std=c++17 -I aircraft/fdm -I /usr/local/include/JSBSim \
      test/fdm_fixtures.cpp aircraft/fdm/jsbsim_adapter.cpp \
      /usr/local/lib/libJSBSim.a -o /tmp/fdmfix -lm -lpthread
  /tmp/fdmfix'
