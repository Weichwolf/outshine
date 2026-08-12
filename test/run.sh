#!/bin/sh
# THE HARNESS. One process per test, one verdict per test, non-zero on anything that is not a pass.
# POSIX shell, and the whole dependency set is the shell, the compiler and the clock the compiler
# builds (test/Millis.cpp) -- a harness that needs a language runtime to say "the build is broken" is
# one more thing that can be broken.
#
#   sh test/run.sh [--timeout SECONDS] [--allow-skip LAYER/NAME]...
#
# THE VERDICT IS THE TRAILER THE REPORTER PRINTED, and the exit status is only cross-checked against
# it. A POSIX status is eight bits: 256 failed claims came back as 0 and passed, 77 came back as
# "skipped", and `(void)Report()` satisfied [[nodiscard]] and returned 0 -- three green runs over a
# red test, all three the same defect, which is trusting a number that cannot hold the answer. A
# trailer that is missing, doubled, malformed or disagreeing with the status is a HARD ERROR naming
# the file, never a pass.
#
# A TEST IS A .cpp IN A LAYER DIRECTORY, and the directory is the whole of the decision. Reading the
# source for `#include "Check.h"` made a forgotten include a silent non-test -- the file simply never
# ran, and the run stayed green. Every .cpp under test/ is now either in a layer, where it is built,
# run and must produce a trailer, or in a directory declared as the Makefile's; a directory in
# neither is a hard error before anything is built.
#
# THE INCLUDE SET COMES FROM THE TEST'S DIRECTORY, mirroring src/. A default include set is the quiet
# failure this whole design is built to avoid: a mistyped directory would get a wider set, and a test
# that passes because it compiled against more than its own layer proves nothing about the layer.
#
# THE MILLISECONDS ON A LINE ARE WHAT THE HARNESS SPENT ON THAT TEST, its build included. The layer's
# objects are compiled once and reused, so the first test of a layer carries what the rest get free.

set -u
# JOB CONTROL, SO EVERY CHILD IS A PROCESS GROUP OF ITS OWN. Without it the watchdog's `sleep` is
# orphaned to init when the watchdog is killed (measured: 8 stray `sleep 120` after two runs of four
# tests), and anything a test itself left running -- a proxy still bound to a port is what has
# already cost a gate run here -- survives the run that started it.
set -m

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 2

# OUTSIDE THE TREE. A build artefact nobody committed on purpose is a file the next round has to make
# a decision about.
BUILD=${TMPDIR:-/tmp}
BUILD=${BUILD%/}/outshine-tests
CXX=${CXX:-c++}
CXXSTD=-std=c++17
WARN="-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter"
OPT=-O2

TIMEOUT_S=120
ALLOWED_SKIPS=""

# THE EXPECT-FAIL SET, AND IT CARRIES THE COUNT. Inverting on "the test was red" would also invert a
# crash, a build that fell over, or a test that failed for two reasons instead of one -- which is how
# a broken harness stays quiet. `LAYER/NAME:FAILURES` demands exactly that many failed claims, and
# the count comes from the trailer, which is the only place that can hold it.
EXPECT_FAIL="harness/ExpectFail:1"

Die() {
  printf 'run.sh: %s\n' "$*" >&2
  exit 2
}

# WHAT AN INTERRUPTED RUN LEAVES: nothing. The test and its watchdog are process groups of their own,
# so a Ctrl-C at the terminal reaches the harness and not them; this trap is what reaches them.
RUNNING_GROUPS=""
KillRunning() {
  for runningGroup in $RUNNING_GROUPS; do
    kill -TERM -"$runningGroup" 2>/dev/null
  done
  RUNNING_GROUPS=""
}
trap 'KillRunning; exit 130' INT
trap 'KillRunning; exit 143' TERM
trap 'KillRunning; exit 129' HUP
trap 'KillRunning' EXIT

while [ $# -gt 0 ]; do
  case "$1" in
    --timeout)
      [ $# -ge 2 ] || Die "--timeout wants a number of seconds"
      TIMEOUT_S=$2
      shift 2
      ;;
    --allow-skip)
      [ $# -ge 2 ] || Die "--allow-skip wants a test name, e.g. world/TilesAnswer"
      ALLOWED_SKIPS="$ALLOWED_SKIPS $2"
      shift 2
      ;;
    *) Die "unknown argument '$1'" ;;
  esac
done

# WHAT A TEST OF THIS LAYER MAY NAME. One line per declared directory; there is no default arm.
LayerIncludes() {
  case "$1" in
    core) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    data) printf '%s' "-Isrc/core -Isrc/data" ;;
    generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    harness) printf '%s' "" ;;
    *) return 1 ;;
  esac
}

# WHAT A TEST OF THIS LAYER LINKS. Layer archives are a later step; until then the harness compiles
# the same source groups the Makefile does, each with its own directory's include set.
LayerGroups() {
  case "$1" in
    core) printf '%s' "src/core src/core/io" ;;
    data) printf '%s' "src/core src/core/io src/data" ;;
    generators/draw) printf '%s' "src/core src/generators src/generators/draw" ;;
    harness) printf '%s' "" ;;
    *) return 1 ;;
  esac
}

# WHAT IS UNDER test/ AND IS NOT THE HARNESS'S. Each of these is built by the Makefile and none of
# them has a reporter or a verdict: an entry point is run by a person, and a compile-judged subject
# is judged by whether it compiles at all. There is no default arm here either.
NotTheHarnesses() {
  case "$1" in
    .) printf '%s' "the harness's own clock" ;;
    clients) printf '%s' "entry points, built by the Makefile" ;;
    host) printf '%s' "host implementations of what the library declares, built by the Makefile" ;;
    generators) printf '%s' "a gate program the Makefile builds and runs" ;;
    compile | compile/*) printf '%s' "judged by whether it compiles, by the Makefile, never run" ;;
    *) return 1 ;;
  esac
}

# A SOURCE COMPILES WITH ITS OWN DIRECTORY'S INCLUDE SET, never with the test's: that is what makes
# the layering the build rather than a rule, and it is the same set the Makefile hands each group.
GroupIncludes() {
  case "$1" in
    src/core | src/core/io) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    src/data) printf '%s' "-Isrc/core -Isrc/data" ;;
    src/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    src/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    *) return 1 ;;
  esac
}

# An object is stale unless it is newer than its source AND newer than every header that compile
# actually included: a `[ obj -nt src ]` guard alone keeps objects built against an old header, and
# every number measured from that binary is a phantom.
UpToDate() {
  objectPath=$1
  sourcePath=$2
  depsPath=${objectPath%.o}.d
  [ -f "$objectPath" ] || return 1
  [ -f "$depsPath" ] || return 1
  if [ "$sourcePath" -nt "$objectPath" ]; then return 1; fi
  for prerequisite in $(sed -e 's/^[^:]*://' -e 's/\\//g' "$depsPath"); do
    [ -e "$prerequisite" ] || return 1
    if [ "$prerequisite" -nt "$objectPath" ]; then return 1; fi
  done
  return 0
}

BuildGroup() {
  group=$1
  groupIncludes=$(GroupIncludes "$group") || Die "no include set declared for the source group $group"
  for unit in "$group"/*.cpp; do
    [ -e "$unit" ] || continue
    unitObject=$BUILD/obj/$(printf '%s' "$group" | tr / -)-$(basename "$unit" .cpp).o
    if ! UpToDate "$unitObject" "$unit"; then
      # shellcheck disable=SC2086
      $CXX "$unit" $CXXSTD $OPT $WARN -MMD -MP $groupIncludes -c -o "$unitObject" || return 1
    fi
    OBJECTS="$OBJECTS $unitObject"
  done
  return 0
}

# The child runs in the background and a watchdog kills it, because macOS has no timeout(1). `wait`
# is what times the run -- polling would add its own interval to every measurement -- and the
# watchdog leaves a marker so that a killed run is TIMEOUT and never a generic failure. Every kill is
# a kill of the GROUP: killing the watchdog alone leaves its `sleep` running under init, and killing
# a test alone leaves whatever the test started.
RunWithTimeout() {
  binary=$1
  log=$2
  marker=$3
  rm -f "$marker"
  "$binary" >"$log" 2>&1 &
  child=$!
  RUNNING_GROUPS=$child
  (
    sleep "$TIMEOUT_S"
    if kill -0 "$child" 2>/dev/null; then
      : >"$marker"
      kill -TERM -"$child" 2>/dev/null
      sleep 2
      kill -KILL -"$child" 2>/dev/null
    fi
  ) >/dev/null 2>&1 &
  watchdog=$!
  RUNNING_GROUPS="$child $watchdog"
  # The shell announces a background job that died on a signal in its own words; the harness reports
  # the signal by name below, and one statement has one place.
  wait "$child" 2>/dev/null
  status=$?
  KillRunning
  wait "$watchdog" 2>/dev/null
  return $status
}

# THE TRAILER, and `set --` is used inside a function so it splits the function's parameters and not
# the run's. Everything the verdict needs comes out of here, or the run dies naming the file.
TRAILER_CHECKS=0
TRAILER_FAILURES=0
TRAILER_SKIPS=0
Number() {
  case "$1" in
    '' | *[!0-9]*) return 1 ;;
    *) return 0 ;;
  esac
}
ReadTrailer() {
  trailerId=$1
  trailerLog=$2
  trailerCount=$(grep -c '^CHECKS ' "$trailerLog")
  [ "$trailerCount" -eq 1 ] ||
    Die "$trailerId printed $trailerCount verdict lines and a verdict is exactly one: a test that emits none did not include the reporter, and one that emits two reported twice -- $trailerLog"
  # shellcheck disable=SC2046
  set -- $(grep '^CHECKS ' "$trailerLog")
  [ $# -eq 6 ] && [ "$1" = CHECKS ] && [ "$3" = FAILURES ] && [ "$5" = SKIPPED ] ||
    Die "$trailerId printed a verdict line the reporter cannot have written -- $trailerLog"
  Number "$2" && Number "$4" && Number "$6" ||
    Die "$trailerId printed a verdict line whose counts are not numbers -- $trailerLog"
  TRAILER_CHECKS=$2
  TRAILER_FAILURES=$4
  TRAILER_SKIPS=$6
  return 0
}

DeclaredFailures() {
  for declared in $EXPECT_FAIL; do
    case "$declared" in "$1":*)
      printf '%s' "${declared##*:}"
      return 0
      ;;
    esac
  done
  return 1
}

SkipAllowed() {
  for allowed in $ALLOWED_SKIPS; do
    [ "$allowed" = "$1" ] && return 0
  done
  return 1
}

mkdir -p "$BUILD/obj" "$BUILD/log" || Die "cannot write under $BUILD"
$CXX test/Millis.cpp $CXXSTD $OPT $WARN -o "$BUILD/millis" || Die "the clock did not build"
Now() { "$BUILD/millis"; }

# EVERY DIRECTORY RESOLVES BEFORE ANYTHING IS BUILT. An undeclared one found halfway through is a
# refusal the reader has to notice under three green lines; found here it is the only thing printed.
TESTS=""
for candidate in $(find test -name '*.cpp' | sort); do
  candidateLayer=$(dirname "${candidate#test/}")
  if LayerIncludes "$candidateLayer" >/dev/null; then
    LayerGroups "$candidateLayer" >/dev/null ||
      Die "$candidate is under test/$candidateLayer, which declares an include set but no source groups"
    TESTS="$TESTS $candidate"
  elif NotTheHarnesses "$candidateLayer" >/dev/null; then
    continue
  else
    Die "$candidate is under test/$candidateLayer, and the harness knows no such directory -- declare it in LayerIncludes and LayerGroups to run its tests, or in NotTheHarnesses if the Makefile judges it"
  fi
done
[ -n "$TESTS" ] && [ "$TESTS" != " " ] || Die "no test under a declared layer of test/"

started=$(Now)
passed=0
failed=0
timedout=0
signalled=0
unbuilt=0
skipped=0
undeclaredSkips=0
inverted=0

for testSource in $TESTS; do
  layer=$(dirname "${testSource#test/}")
  name=$(basename "$testSource" .cpp)
  id="$layer/$name"
  includes=$(LayerIncludes "$layer") || Die "test/$layer stopped resolving between the check and the build"
  groups=$(LayerGroups "$layer") || Die "test/$layer stopped resolving between the check and the build"

  log=$BUILD/log/$(printf '%s' "$id" | tr / -).log
  binary=$BUILD/$(printf '%s' "$id" | tr / -)
  marker=$binary.timeout

  before=$(Now)
  OBJECTS=""
  built=yes
  : >"$log"
  for group in $groups; do
    BuildGroup "$group" >>"$log" 2>&1 || built=no
  done
  if [ "$built" = yes ]; then
    # shellcheck disable=SC2086
    $CXX "$testSource" $OBJECTS $CXXSTD $OPT $WARN -Itest $includes -o "$binary" >>"$log" 2>&1 || built=no
  fi

  # A BUILD FAILURE, A TIMEOUT AND A SIGNAL ARE JUDGED BEFORE THE TRAILER, because none of the three
  # can be expected to have printed one -- and a test that crashed after printing one would otherwise
  # be read out of its own corpse.
  failures=0
  skips=0
  if [ "$built" = no ]; then
    verdict=BUILD
  else
    RunWithTimeout "$binary" "$log" "$marker"
    status=$?
    if [ -f "$marker" ]; then
      verdict=TIMEOUT
    elif [ "$status" -ge 128 ]; then
      verdict=SIGNAL
    else
      ReadTrailer "$id" "$log"
      failures=$TRAILER_FAILURES
      skips=$TRAILER_SKIPS
      expected=0
      [ "$failures" -eq 0 ] || expected=1
      [ "$status" -eq "$expected" ] ||
        Die "$id reported FAILURES $failures and exited $status, which do not agree: the reporter's answer was discarded, altered, or never returned -- $log"
      if [ "$failures" -gt 0 ]; then
        verdict=FAIL
      elif [ "$skips" -gt 0 ]; then
        verdict=SKIP
      else
        verdict=PASS
      fi
    fi
  fi

  if wanted=$(DeclaredFailures "$id"); then
    if [ "$verdict" = FAIL ] && [ "$failures" -eq "$wanted" ]; then
      verdict=PASS
      inverted=$((inverted + 1))
    else
      printf 'run.sh: %s is declared to fail %s claim(s) and reported %s (%s failed)\n' \
        "$id" "$wanted" "$verdict" "$failures" >&2
      verdict=FAIL
    fi
  fi

  elapsed=$(( $(Now) - before ))
  case "$verdict" in
    PASS) passed=$((passed + 1)) ;;
    FAIL) failed=$((failed + 1)) ;;
    TIMEOUT)
      timedout=$((timedout + 1))
      printf 'run.sh: %s was killed after %s s\n' "$id" "$TIMEOUT_S" >&2
      ;;
    SIGNAL)
      signalled=$((signalled + 1))
      printf 'run.sh: %s died on SIG%s\n' "$id" "$(kill -l $((status - 128)) 2>/dev/null)" >&2
      ;;
    BUILD) unbuilt=$((unbuilt + 1)) ;;
    SKIP)
      skipped=$((skipped + 1))
      if ! SkipAllowed "$id"; then
        undeclaredSkips=$((undeclaredSkips + 1))
        printf 'run.sh: %s skipped and no --allow-skip %s was given\n' "$id" "$id" >&2
      fi
      ;;
  esac

  printf '%-7s %-46s %6s ms  %s\n' "$verdict" "$id" "$elapsed" "$log"
done

total=$((passed + failed + timedout + signalled + unbuilt + skipped))
printf '%s tests: %s PASS  %s FAIL  %s TIMEOUT  %s SIGNAL  %s BUILD  %s SKIP  in %s ms\n' \
  "$total" "$passed" "$failed" "$timedout" "$signalled" "$unbuilt" "$skipped" \
  "$(( $(Now) - started ))"
[ "$inverted" -gt 0 ] && printf 'expect-fail inverted: %s\n' "$EXPECT_FAIL"

# A SKIP IS RED UNLESS IT WAS DECLARED ON THE COMMAND LINE. A silent skip is the defect class this
# repository keeps finding, wearing a harness's hat.
red=$((failed + timedout + signalled + unbuilt + undeclaredSkips))
[ "$red" -eq 0 ] || exit 1
exit 0
