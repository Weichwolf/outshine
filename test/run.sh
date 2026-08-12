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
# THE INCLUDE SET COMES FROM THE TEST'S DIRECTORY. A default include set is the quiet
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
#
# THREE SUITES, SPLIT BY INSTRUMENT (doc/requirements.md I.26.9). test/unit/ mirrors src/ exactly and
# is the only tree that carries the layering proof; test/render/ and test/scenario/ are declarative
# and organised by feature and by declared run. A render case links the library ENTIRE by
# construction -- it needs the reader, the renderer and the readback at once -- so `render` gets the
# union of the layer sets and must never look like a mirror of one directory.
LayerIncludes() {
  case "$1" in
    unit/core) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    unit/data) printf '%s' "-Isrc/core -Isrc/data" ;;
    unit/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    unit/scenario) printf '%s' "-Isrc/core -Isrc/scenario" ;;
    unit/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    unit/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    unit/world) printf '%s' "-Isrc/core -Isrc/data -Isrc/world -Isrc/world/tiles" ;;
    unit/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    unit/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    unit/clients) printf '%s' "-Isrc/clients" ;;
    harness) printf '%s' "" ;;
    render) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients" ;;
    *) return 1 ;;
  esac
}

# WHAT A LAYER COMPILES AND LINKS WITH BEYOND ITS INCLUDES, and there are exactly two answers. Every
# unit layer wants nothing: the shell, the compiler and the clock are the whole dependency set, which
# is what lets this harness report "the build is broken" without needing the build.
#
# `render` IS THE WIDEST EXCEPTION AND IT IS NOT A CHOICE. A render case's verdict is "our pixels agree
# with Cycles", so its subject is the renderer -- the device, the pass topology, the raster
# convention. Anything that stood in for them would be a second rasteriser scoring itself. So this
# layer takes the renderer's own toolchain: C++20, native Dawn, SDL3_image for the image boundary,
# and the platform frameworks Metal needs.
DAWN_OUT=vendor/dawn/out
DAWN_LIBDIR=$DAWN_OUT/src/dawn/native
LayerToolchain() {
  case "$1" in
    render) printf '%s' "-std=c++20 -isystem $DAWN_OUT/gen/include -isystem vendor/dawn/include -isystem vendor" ;;
    unit/clients) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3-image)" ;;
    *) printf '%s' "$CXXSTD" ;;
  esac
}
# THE SANITISERS ARE AN INSTRUMENT AND A LAYER DECLARES WHETHER IT WANTS ONE. `render` does, and it
# is the only layer that talks to a GPU API with hand-managed buffers, staging copies and mapped
# ranges -- the exact places a use-after-free lives. -fno-sanitize-recover is what makes
# UndefinedBehaviorSanitizer a verdict rather than a log: without it a signed overflow prints and the
# run exits 0. THE INSTRUMENTED OBJECTS LIVE IN THEIR OWN DIRECTORY, so an instrumented object can
# never be linked into the binary the comparison is taken from, and the instrument appears in the
# test's own id so a sanitised run can never be read as a shipping one.
LayerSanitiser() {
  case "$1" in
    render) printf '%s' "-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1" ;;
    *) printf '%s' "" ;;
  esac
}

LayerLink() {
  case "$1" in
    render)
      platform=""
      if [ "$(uname -s)" = Darwin ]; then
        platform="-framework Cocoa -framework IOKit -framework Foundation -framework IOSurface -framework QuartzCore -framework Metal"
      fi
      printf '%s' "-L$DAWN_LIBDIR -lwebgpu_dawn $(pkg-config --libs sdl3-image) -lpthread -ldl -lm $platform"
      ;;
    unit/clients) printf '%s' "$(pkg-config --libs sdl3-image)" ;;
    *) printf '%s' "" ;;
  esac
}

# WHAT A TEST OF THIS LAYER LINKS. Layer archives are a later step; until then the harness compiles
# the same source groups the Makefile does, each with its own directory's include set.
#
# `unit/world` LINKS NOTHING and that is a limit, not a choice: src/world/tiles decodes terrarium PNG
# and imagery JPEG through SDL3_image, whose flags come from pkg-config -- and this harness's whole
# dependency set is the shell, the compiler and the clock. A test that needs to RUN world code is
# what pays to widen that.
#
# `unit/clients` PAID IT, for one file. src/clients/Image.cpp IS the SDL3_image boundary -- the decode
# a glTF base-colour texture arrives through and the encode a render case's pictures leave through --
# so a test of it that stood in for the library would be testing the stand-in. The widening is one
# layer and one source and it changes nothing for the others: every remaining unit layer still links
# nothing at all.
LayerGroups() {
  case "$1" in
    unit/core) printf '%s' "src/core src/core/io" ;;
    unit/data) printf '%s' "src/core src/core/io src/data" ;;
    unit/gltf) printf '%s' "src/core src/gltf" ;;
    unit/scenario) printf '%s' "src/core src/scenario" ;;
    unit/generators) printf '%s' "src/core src/generators" ;;
    unit/generators/draw) printf '%s' "src/core src/generators src/generators/draw" ;;
    unit/world) printf '%s' "" ;;
    unit/render/plan) printf '%s' "src/core src/core/io src/render/plan" ;;
    unit/render/draw) printf '%s' "src/core src/core/io src/render/draw" ;;
    unit/clients) printf '%s' "src/clients/Image.cpp" ;;
    harness) printf '%s' "" ;;
    render) printf '%s' "src/core src/core/io src/gltf src/render/plan src/render/draw src/render src/render/stages src/clients/GltfStudio.cpp src/clients/Image.cpp" ;;
    *) return 1 ;;
  esac
}

# WHAT ONE TEST SOURCE IS RUN OVER. Every layer but `render` runs its binary once, with no argument.
# A RENDER CASE IS A DIRECTORY (doc/requirements.md I.26.10): the runner is built once and invoked
# once per case directory with the directory as its argument, so what is shared is the CODE and never
# the process -- still one process and one real verdict per case, and a crash in case 137 fails case
# 137 and nothing else.
LayerCases() {
  case "$1" in
    render) find test/render -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    *) printf '%s' "" ;;
  esac
}

# WHAT IS UNDER test/ AND IS NOT THE HARNESS'S. Each of these is built by the Makefile and none of
# them has a reporter or a verdict: an entry point is run by a person, and a compile-judged subject
# is judged by whether it compiles at all. There is no default arm here either.
NotTheHarnesses() {
  case "$1" in
    .) printf '%s' "the harness's own clock" ;;
    host) printf '%s' "host implementations of what the library declares, compiled into the library" ;;
    unit/compile | unit/compile/*) printf '%s' "a compile subject, judged by the layer's own refusal test, never linked" ;;
    *) return 1 ;;
  esac
}

# A SOURCE COMPILES WITH ITS OWN DIRECTORY'S INCLUDE SET, never with the test's: that is what makes
# the layering the build rather than a rule, and it is the same set the Makefile hands each group.
GroupIncludes() {
  case "$1" in
    src/core | src/core/io) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    src/data) printf '%s' "-Isrc/core -Isrc/data" ;;
    src/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    src/scenario) printf '%s' "-Isrc/core -Isrc/scenario" ;;
    src/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    src/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    src/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    src/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    src/render | src/render/stages) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages" ;;
    src/clients/GltfStudio.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients" ;;
    src/clients/Image.cpp) printf '%s' "-Isrc/clients $(pkg-config --cflags sdl3-image)" ;;
    *) return 1 ;;
  esac
}

# A SOURCE THAT NEEDS MORE THAN THE HOUSE STANDARD SAYS SO ONCE. The renderer is C++20 and speaks to
# Dawn; nothing else in the tree is either.
GroupToolchain() {
  case "$1" in
    src/render | src/render/stages | src/clients/GltfStudio.cpp) LayerToolchain render ;;
    *) printf '%s' "$CXXSTD" ;;
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

# A GROUP IS A DIRECTORY, OR ONE NAMED FILE. The second form exists because a render case needs the
# PNG encoder and nothing else out of src/clients, and pulling the whole directory in would link the
# world simulation into a test whose subject is one triangle.
OBJDIR=$BUILD/obj

BuildGroup() {
  group=$1
  groupIncludes=$(GroupIncludes "$group") || Die "no include set declared for the source group $group"
  groupStd=$(GroupToolchain "$group")
  case "$group" in
    *.cpp) groupUnits=$group ;;
    *) groupUnits=$(find "$group" -maxdepth 1 -name '*.cpp' | sort) ;;
  esac
  for unit in $groupUnits; do
    [ -e "$unit" ] || continue
    unitObject=$OBJDIR/$(dirname "$unit" | tr / -)-$(basename "$unit" .cpp).o
    if ! UpToDate "$unitObject" "$unit"; then
      # shellcheck disable=SC2086
      $CXX "$unit" $groupStd $OPT $WARN $SAN -MMD -MP $groupIncludes -c -o "$unitObject" || return 1
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
  runArgument=$4
  rm -f "$marker"
  if [ -n "$runArgument" ]; then
    "$binary" "$runArgument" >"$log" 2>&1 &
  else
    "$binary" >"$log" 2>&1 &
  fi
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
TRAILER_UNPREPARED=0
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
  [ $# -eq 8 ] && [ "$1" = CHECKS ] && [ "$3" = FAILURES ] && [ "$5" = SKIPPED ] &&
    [ "$7" = UNPREPARED ] ||
    Die "$trailerId printed a verdict line the reporter cannot have written -- $trailerLog"
  Number "$2" && Number "$4" && Number "$6" && Number "$8" ||
    Die "$trailerId printed a verdict line whose counts are not numbers -- $trailerLog"
  TRAILER_CHECKS=$2
  TRAILER_FAILURES=$4
  TRAILER_SKIPS=$6
  TRAILER_UNPREPARED=$8
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

mkdir -p "$BUILD/obj" "$BUILD/obj-sanitised" "$BUILD/log" || Die "cannot write under $BUILD"
SAN=""
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
unprepared=0
undeclaredSkips=0
inverted=0

# ONE INVOCATION, RUN AND JUDGED. It is a function because a render source is invoked once per case
# directory and every other source once with no argument: the judgement must be the SAME judgement,
# and a second copy of it inside the render arm is how the two would come to disagree.
#   $1 the id printed and judged under   $2 the binary   $3 the argument, or empty
Judge() {
  judgeId=$1
  judgeBinary=$2
  judgeArgument=$3
  log=$BUILD/log/$(printf '%s' "$judgeId" | tr / -).log
  marker=$judgeBinary.timeout
  before=$(Now)

  failures=0
  skips=0
  unpreparedHere=0
  RunWithTimeout "$judgeBinary" "$log" "$marker" "$judgeArgument"
  status=$?
  if [ -f "$marker" ]; then
    verdict=TIMEOUT
  elif [ "$status" -ge 128 ]; then
    verdict=SIGNAL
  else
    ReadTrailer "$judgeId" "$log"
    failures=$TRAILER_FAILURES
    skips=$TRAILER_SKIPS
    unpreparedHere=$TRAILER_UNPREPARED
    expected=0
    { [ "$failures" -eq 0 ] && [ "$unpreparedHere" -eq 0 ]; } || expected=1
    [ "$status" -eq "$expected" ] ||
      Die "$judgeId reported FAILURES $failures UNPREPARED $unpreparedHere and exited $status, which do not agree: the reporter's answer was discarded, altered, or never returned -- $log"
    if [ "$failures" -gt 0 ]; then
      verdict=FAIL
    elif [ "$unpreparedHere" -gt 0 ]; then
      verdict=UNPREP
    elif [ "$skips" -gt 0 ]; then
      verdict=SKIP
    else
      verdict=PASS
    fi
  fi
  Record "$judgeId" "$(( $(Now) - before ))"
}

# WHAT A VERDICT DOES TO THE RUN. Separate from Judge so that a build failure -- which prints no
# trailer and can therefore not be judged from one -- is recorded by exactly the same counter.
Record() {
  recordId=$1
  recordMs=$2
  if wanted=$(DeclaredFailures "$recordId"); then
    if [ "$verdict" = FAIL ] && [ "$failures" -eq "$wanted" ]; then
      verdict=PASS
      inverted=$((inverted + 1))
    else
      printf 'run.sh: %s is declared to fail %s claim(s) and reported %s (%s failed)\n' \
        "$recordId" "$wanted" "$verdict" "$failures" >&2
      verdict=FAIL
    fi
  fi

  case "$verdict" in
    PASS) passed=$((passed + 1)) ;;
    FAIL) failed=$((failed + 1)) ;;
    TIMEOUT)
      timedout=$((timedout + 1))
      printf 'run.sh: %s was killed after %s s\n' "$recordId" "$TIMEOUT_S" >&2
      ;;
    SIGNAL)
      signalled=$((signalled + 1))
      printf 'run.sh: %s died on SIG%s\n' "$recordId" "$(kill -l $((status - 128)) 2>/dev/null)" >&2
      ;;
    BUILD) unbuilt=$((unbuilt + 1)) ;;
    UNPREP)
      unprepared=$((unprepared + 1))
      printf 'run.sh: %s has no prepared input -- run test/corpus/prepare.py\n' "$recordId" >&2
      ;;
    SKIP)
      skipped=$((skipped + 1))
      if ! SkipAllowed "$recordId"; then
        undeclaredSkips=$((undeclaredSkips + 1))
        printf 'run.sh: %s skipped and no --allow-skip %s was given\n' "$recordId" "$recordId" >&2
      fi
      ;;
  esac

  printf '%-7s %-46s %6s ms  %s\n' "$verdict" "$recordId" "$recordMs" "$log"
}

for testSource in $TESTS; do
  layer=$(dirname "${testSource#test/}")
  name=$(basename "$testSource" .cpp)
  id="$layer/$name"
  includes=$(LayerIncludes "$layer") || Die "test/$layer stopped resolving between the check and the build"
  groups=$(LayerGroups "$layer") || Die "test/$layer stopped resolving between the check and the build"
  toolchain=$(LayerToolchain "$layer")
  linkage=$(LayerLink "$layer")

  log=$BUILD/log/$(printf '%s' "$id" | tr / -).log
  binary=$BUILD/$(printf '%s' "$id" | tr / -)

  before=$(Now)
  OBJECTS=""
  built=yes
  : >"$log"
  for group in $groups; do
    BuildGroup "$group" >>"$log" 2>&1 || built=no
  done
  # THE COMPILE COMMAND A LAYER'S REFUSAL TEST INVOKES, handed to it rather than written down twice.
  # A test that proves `Renderer.h` has no spelling in src/scenario must compile a subject with the
  # scenario layer's include set -- and if that set were stated a second time inside the test, the
  # day the two disagree is the day the proof stops being about the build. It is the SAME string this
  # test itself is compiled with, so a layer that widened cannot widen for its subjects only.
  compileDefine="-DOUTSHINE_COMPILE=\"$CXX $CXXSTD $WARN $includes\""
  if [ "$built" = yes ]; then
    # shellcheck disable=SC2086
    $CXX "$testSource" $OBJECTS $toolchain $OPT $WARN -Itest $includes "$compileDefine" $linkage -o "$binary" >>"$log" 2>&1 || built=no
  fi

  # A BUILD FAILURE IS JUDGED BEFORE ANY TRAILER, because a binary that does not exist cannot print
  # one -- and a test that crashed after printing one would otherwise be read out of its own corpse.
  if [ "$built" = no ]; then
    failures=0
    skips=0
    verdict=BUILD
    Record "$id" "$(( $(Now) - before ))"
    continue
  fi

  cases=$(LayerCases "$layer")
  if [ -z "$cases" ]; then
    Judge "$id" "$binary" ""
    continue
  fi
  # A DECLARATIVE SUITE WITH NO DECLARATION IS THE VACUOUS GATE IN ITS PUREST FORM: a runner that
  # runs over nothing and reports nothing, green. The enumeration is the tracked manifests, so this
  # can only be empty if the suite itself is.
  for caseDirectory in $cases; do
    Judge "${caseDirectory#test/}" "$binary" "$caseDirectory"
  done

  sanitiser=$(LayerSanitiser "$layer")
  [ -n "$sanitiser" ] || continue
  before=$(Now)
  OBJDIR=$BUILD/obj-sanitised
  SAN=$sanitiser
  OBJECTS=""
  built=yes
  sanitisedLog=$BUILD/log/$(printf '%s' "$id" | tr / -)-sanitised.log
  : >"$sanitisedLog"
  for group in $groups; do
    BuildGroup "$group" >>"$sanitisedLog" 2>&1 || built=no
  done
  sanitisedBinary=$binary.sanitised
  if [ "$built" = yes ]; then
    # shellcheck disable=SC2086
    $CXX "$testSource" $OBJECTS $toolchain $OPT $WARN $SAN -Itest $includes "$compileDefine" $linkage -o "$sanitisedBinary" >>"$sanitisedLog" 2>&1 || built=no
  fi
  OBJDIR=$BUILD/obj
  SAN=""
  if [ "$built" = no ]; then
    failures=0
    skips=0
    verdict=BUILD
    Record "$id~sanitised" "$(( $(Now) - before ))"
    continue
  fi
  # DETECT_STACK_USE_AFTER_RETURN IS PART OF THE INSTRUMENT: a build that carries the
  # instrumentation still reports nothing about it without this line.
  ASAN_OPTIONS=detect_stack_use_after_return=1
  UBSAN_OPTIONS=print_stacktrace=1
  export ASAN_OPTIONS UBSAN_OPTIONS
  for caseDirectory in $cases; do
    Judge "${caseDirectory#test/}~sanitised" "$sanitisedBinary" "$caseDirectory"
    if grep -qE "AddressSanitizer|runtime error:" "$log"; then
      printf 'run.sh: %s -- the run finished and the sanitiser spoke, %s\n' \
        "${caseDirectory#test/}~sanitised" "$log" >&2
      failed=$((failed + 1))
    fi
  done
  unset ASAN_OPTIONS UBSAN_OPTIONS
done

total=$((passed + failed + timedout + signalled + unbuilt + skipped + unprepared))
printf '%s tests: %s PASS  %s FAIL  %s TIMEOUT  %s SIGNAL  %s BUILD  %s SKIP  %s UNPREPARED  in %s ms\n' \
  "$total" "$passed" "$failed" "$timedout" "$signalled" "$unbuilt" "$skipped" "$unprepared" \
  "$(( $(Now) - started ))"
[ "$inverted" -gt 0 ] && printf 'expect-fail inverted: %s\n' "$EXPECT_FAIL"

# A SKIP IS RED UNLESS IT WAS DECLARED ON THE COMMAND LINE, AND AN UNPREPARED CASE IS RED WITH NO
# WAY TO DECLARE IT AWAY. A silent skip is the defect class this repository keeps finding, wearing a
# harness's hat; an unprepared corpus wearing a skip's hat would be the same defect one level up --
# a tier that skips when its inputs are absent cannot be told from a tier that passed having tested
# nothing.
red=$((failed + timedout + signalled + unbuilt + undeclaredSkips + unprepared))
[ "$red" -eq 0 ] || exit 1
exit 0
