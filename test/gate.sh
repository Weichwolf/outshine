#!/bin/sh
# The fast gate: the smallest set that still judges from OUTSIDE this tree, under a minute.
# It runs nothing itself -- test/run.sh is the only test runner and this names a subset of it.
#
# SUITES ARE NAMED TOGETHER, not one run each. Every `run.sh` invocation pays a start: it builds
# the clock, resolves the corpus and reads the declarations. Six starts bought six copies of that
# and nothing else. Khronos stays apart because run.sh refuses a single manifest case beside
# another suite, and that refusal is right.
set -u
cd "$(dirname "$0")/.." || exit 1

said=${TMPDIR:-/tmp}/outshine-gate-step.txt
began=$(date +%s)
red=0
Step() {
  name=$1
  shift
  at=$(date +%s)
  if "$@" > "$said" 2>&1; then verdict=ok; else verdict=RED; red=1; fi
  printf '%-38s %3ds  %s  %s\n' "$name" "$(($(date +%s) - at))" "$verdict" \
    "$(grep -h 'tests:' "$said" | head -1 | cut -c1-30)"
  [ "$verdict" = RED ] && tail -20 "$said"
  return 0
}

Step 'the library and its clients' make
Step 'the tiers and what stands wider'  sh test/run.sh --audit-layers --audit-access
Step 'khronos static'               sh test/run.sh khronos/glTF/WaterBottle
Step 'khronos animated'             sh test/run.sh khronos/glTF/BoxAnimated
Step 'the simulation, content, mix'  sh test/run.sh outshine/physics outshine/content outshine/audio
Step 'the client drives'            ./build/outshine-driver --headless --offline --frames 8

printf '\n%s in %ds\n' "$([ "$red" = 0 ] && echo GREEN || echo RED)" "$(($(date +%s) - began))"
printf 'NOT covered here: the engine submission path (outshine/door, 27 cases, ~100s),\n'
printf 'the validator, wpt, test262, the render corpus and the claims. A change to SubjectProxy,\n'
printf 'Live or the renderer wants outshine/door as well; a full verdict is test/run.sh.\n'
exit $red
