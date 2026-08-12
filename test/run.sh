#!/bin/sh
# THE HARNESS. One process per test, one verdict per test, non-zero on anything that is not a pass.
# POSIX shell, and the whole dependency set is the shell, the compiler and the clock the compiler
# builds (test/Millis.cpp) -- a harness that needs a language runtime to say "the build is broken" is
# one more thing that can be broken.
#
#   sh test/run.sh [--timeout SECONDS] [--allow-skip LAYER/NAME]...
#
# A TEST IS A TRANSLATION UNIT THAT INCLUDES THE REPORTER. Discovery is that property and not a list:
# a list rots the day someone adds a file, and the reporter is what a test needs in order to have a
# verdict at all. The entry points and the must-not-compile negatives under test/ include no reporter
# and are therefore not tests -- they are built and judged by the Makefile until the steps that
# replace them.
#
# THE INCLUDE SET COMES FROM THE TEST'S DIRECTORY, mirroring src/, and AN UNKNOWN DIRECTORY IS A HARD
# ERROR. A default include set is the quiet failure this whole design is built to avoid: a mistyped
# directory would get a wider set, and a test that passes because it compiled against more than its
# own layer proves nothing about the layer.
#
# THE MILLISECONDS ON A LINE ARE WHAT THE HARNESS SPENT ON THAT TEST, its build included. The layer's
# objects are compiled once and reused, so the first test of a layer carries what the rest get free.

set -u

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

# THE EXPECT-FAIL SET, AND IT CARRIES THE COUNT. Inverting on "non-zero" would also invert a crash, a
# build that fell over, or a test that failed for two reasons instead of one -- which is how a broken
# harness stays quiet. `LAYER/NAME:FAILURES` demands exactly that many failed claims.
EXPECT_FAIL="harness/ExpectFail:1"

Die() {
  printf 'run.sh: %s\n' "$*" >&2
  exit 2
}

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
    generators/draw) printf '%s' "src/core src/generators src/generators/draw" ;;
    harness) printf '%s' "" ;;
    *) return 1 ;;
  esac
}

# A SOURCE COMPILES WITH ITS OWN DIRECTORY'S INCLUDE SET, never with the test's: that is what makes
# the layering the build rather than a rule, and it is the same set the Makefile hands each group.
GroupIncludes() {
  case "$1" in
    src/core | src/core/io) printf '%s' "-Isrc/core -Isrc/core/io" ;;
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
# watchdog leaves a marker so that a killed run is TIMEOUT and never a generic failure.
RunWithTimeout() {
  binary=$1
  log=$2
  marker=$3
  rm -f "$marker"
  "$binary" >"$log" 2>&1 &
  child=$!
  (
    sleep "$TIMEOUT_S"
    if kill -0 "$child" 2>/dev/null; then
      : >"$marker"
      kill -TERM "$child" 2>/dev/null
      sleep 2
      kill -KILL "$child" 2>/dev/null
    fi
  ) >/dev/null 2>&1 &
  watchdog=$!
  # The shell announces a background job that died on a signal in its own words; the harness reports
  # the signal by name below, and one statement has one place.
  wait "$child" 2>/dev/null
  status=$?
  kill "$watchdog" 2>/dev/null
  wait "$watchdog" 2>/dev/null
  return $status
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

TESTS=""
for candidate in $(find test -name '*.cpp' | sort); do
  if grep -q '^#include "Check.h"' "$candidate"; then TESTS="$TESTS $candidate"; fi
done
[ -n "$TESTS" ] && [ "$TESTS" != " " ] || Die "no test under test/ includes the reporter"

# EVERY LAYER RESOLVES BEFORE ANYTHING IS BUILT. An undeclared directory found halfway through is a
# refusal the reader has to notice under three green lines; found here it is the only thing printed.
for candidate in $TESTS; do
  candidateLayer=$(dirname "${candidate#test/}")
  LayerIncludes "$candidateLayer" >/dev/null ||
    Die "$candidate is under test/$candidateLayer, which declares no include set -- declare it in LayerIncludes and LayerGroups, or move the test"
  LayerGroups "$candidateLayer" >/dev/null ||
    Die "$candidate is under test/$candidateLayer, which declares no source groups"
done

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

  if [ "$built" = no ]; then
    verdict=BUILD
    status=0
  else
    RunWithTimeout "$binary" "$log" "$marker"
    status=$?
    if [ -f "$marker" ]; then
      verdict=TIMEOUT
    elif [ "$status" -ge 128 ]; then
      verdict=SIGNAL
    elif [ "$status" -eq 77 ]; then
      verdict=SKIP
    elif [ "$status" -eq 0 ]; then
      verdict=PASS
    else
      verdict=FAIL
    fi
  fi

  if wanted=$(DeclaredFailures "$id"); then
    if [ "$verdict" = FAIL ] && [ "$status" -eq "$wanted" ]; then
      verdict=PASS
      inverted=$((inverted + 1))
    else
      printf 'run.sh: %s is declared to fail %s claim(s) and reported %s (%s)\n' \
        "$id" "$wanted" "$verdict" "$status" >&2
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
