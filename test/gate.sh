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
Step 'the simulation, content, mix'  sh test/run.sh outshine/physics outshine/content outshine/audio outshine/scenario outshine/geo outshine/fuzz
Step 'the places on Earth, rendered into build/places'  sh test/run.sh outshine/places

printf '\n%s in %ds\n' "$([ "$red" = 0 ] && echo GREEN || echo RED)" "$(($(date +%s) - began))"
# THE COVERAGE LINE COUNTS RATHER THAN REMEMBERS. It said 27 while the suite held 29, and a
# stale number in a statement about what is NOT covered is the same defect as a stale number in
# one about what is.
printf 'NOT covered here: the engine submission path (outshine/door, %s cases),\n' \
  "$(find test/outshine/door -name '*.cpp' | wc -l | tr -d ' ')" 
printf 'the validator, wpt, test262 and the render corpus. AND NO apps/ RUNS HERE:\n'
printf 'a client is a product, not a check. The one that stood here held this gate for ten\n'
printf 'minutes when it hung and left three processes behind that poisoned every later run.\n'
printf 'test/run.sh apps is where a client is run; test/outshine/places is where one is measured.\n'
printf 'A change to SubjectProxy,\n'
printf 'Live or the renderer wants outshine/door as well -- and so does ANY change to include/,\n'
printf 'because the door cases are the only ones that compile against it.\n'
printf 'AND ANY CHANGE UNDER src/ OR include/ WANTS `make lint`: it holds\n'
printf 'the rules about the SOURCE -- no comment, one spelling per type, no block on the frame\n'
printf 'path, every board edge landing -- and four of them stood red behind a GREEN gate here\n'
printf 'because nothing in this list looks at a source file. It costs two minutes, which is why\n'
printf 'it is named rather than run. A full verdict is test/run.sh.\n'

# READING STATE.md AFTER A RUN IS A STANDING OBLIGATION, so the gate hands it over rather than
# trusting me to remember. Step one is `make`, which regenerates STATE.md, so the table below is
# this run's. What it prints is the WEAKEST measured distance to RAGE and Unreal and every tick
# whose proof the tree does not hold -- the two places the next item comes from.
if [ -f STATE.md ]; then
  printf '\nWeakest against RAGE and Unreal, from this run STATE.md:\n'
  awk -F'|' '/^\| `/ && $4 ~ /%/ {
      share = $4; gsub(/[ %]/, "", share)
      if (share ~ /^[0-9]+$/) { printf "%4d%%  %-14s %s%s\n", share, $2, $6, ($5 ~ /[0-9]/ ? "  ACTIVE:" $5 : "") }
    }' STATE.md | sort -n | head -3
  awk '/^Ticked, but the named proof/ { on = 1 } on && /^- / { print } /^## / { on = 0 }' STATE.md
fi
exit $red
