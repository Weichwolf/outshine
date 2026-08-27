#!/bin/sh
# The fast gate: the smallest set that still judges from OUTSIDE this tree, under a minute.
# It runs nothing itself -- test/run.sh is the only test runner and this names a subset of it.
set -u
cd "$(dirname "$0")/.." || exit 1

began=$(date +%s)
red=0
Step() {
  name=$1
  shift
  at=$(date +%s)
  if "$@" > /tmp/gate-step.txt 2>&1; then said=ok; else said=RED; red=1; fi
  printf '%-34s %3ds  %s  %s\n' "$name" "$(($(date +%s) - at))" "$said" \
    "$(grep -h 'tests:' /tmp/gate-step.txt | head -1 | cut -c1-34)"
  [ "$said" = RED ] && tail -20 /tmp/gate-step.txt
  return 0
}

Step 'the library and its clients' make
Step 'tiers'                       sh test/run.sh --audit-layers
Step 'access'                      sh test/run.sh --audit-access
Step 'khronos static'              sh test/run.sh khronos/glTF/WaterBottle
Step 'khronos animated'            sh test/run.sh khronos/glTF/BoxAnimated
Step 'the simulation'              sh test/run.sh outshine/physics
Step 'the content'                 sh test/run.sh outshine/content
Step 'the mix'                     sh test/run.sh outshine/audio
Step 'the client drives'           ./build/outshine-driver --headless --offline --frames 8

printf '\n%s in %ds\n' "$([ "$red" = 0 ] && echo GREEN || echo RED)" "$(($(date +%s) - began))"
printf 'NOT covered here: the engine submission path (outshine/door, 27 cases, ~100s),\n'
printf 'the validator, wpt, test262, the render corpus and the claims. A change to SubjectProxy,\n'
printf 'Live or the renderer wants outshine/door as well; a full verdict is test/run.sh.\n'
exit $red
