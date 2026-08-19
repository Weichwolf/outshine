#!/bin/sh
# THE HARNESS. One process per test, one verdict per test, non-zero on anything that is not a pass.
# POSIX shell, and the whole dependency set is the shell, the compiler and the clock the compiler
# builds (test/harness/shared/Millis.cpp) -- a harness that needs a language runtime to say "the build is broken" is
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
# WHERE A CASE'S PREPARED FILES LIVE, AND IT IS NOT THE TREE. `CLAUDE.md`: every artefact goes to the
# system temp directory, never into the tree. The leaf is the case's own path with its separators
# flattened, which is the SAME rule `prepare.py` derives -- two spellings of one mapping, and each is
# derivable from the other end without a table.
PREPARED=${TMPDIR:-/tmp}
PREPARED=${PREPARED%/}/outshine-prepared
CXX=${CXX:-c++}
CXXSTD=-std=c++17
WARN="-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter"
OPT=-O2

TIMEOUT_S=120
ALLOWED_SKIPS=""
EXTRA_DEFINES=""
validatedRan=no

# THE EXPECT-FAIL SET, AND IT CARRIES THE COUNT. Inverting on "the test was red" would also invert a
# crash, a build that fell over, or a test that failed for two reasons instead of one -- which is how
# a broken harness stays quiet. `LAYER/NAME:FAILURES` demands exactly that many failed claims, and
# the count comes from the trailer, which is the only place that can hold it.
EXPECT_FAIL="outshine/harness/ExpectFail:1"

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

SUITE=
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
    -*) Die "unknown option '$1'" ;;
    # A SUITE IS A FOLDER, SO SELECTING ONE IS ITS PATH AND NOT A NAME THIS SCRIPT KEEPS A LIST OF.
    # `khronos/glTF` runs that corpus; `outshine/unit` runs every unit layer under it; no argument
    # runs everything. A prefix that matches no declared suite is a refusal rather than an empty run,
    # because "0 tests, 0 failures" reads exactly like success.
    *) SUITE=${1%/}; shift; continue ;;
  esac
done

# WHAT A TEST OF THIS LAYER MAY NAME. One line per declared directory; there is no default arm.
#
# SUITES, SPLIT BY INSTRUMENT (board:0082). test/outshine/unit/ mirrors src/ exactly and
# is the only tree that carries the layering proof; test/render/ and test/scenario/ are declarative
# and organised by feature and by declared run. A render case links the library ENTIRE by
# construction -- it needs the reader, the renderer and the readback at once -- so `render` gets the
# union of the layer sets and must never look like a mirror of one directory.
#
# test/shader/ IS THE FOURTH, AND IT IS THE SAME SPLIT APPLIED ONCE MORE: its subject is shader text and
# its instrument is a real device with no asset, no camera and no oracle. It cannot be a unit layer --
# `outshine/unit/render/stages` links nothing and brings no device up, which is a property of the shading model
# as a header of pure functions and worth keeping -- and it cannot be a render case, because a render
# source is invoked once per case directory and this one has no case to be invoked over.
#
# test/frame/ IS THE FIFTH, AND ITS SUBJECT IS TIME. It links what `render` links because it needs the
# same device and the same subject reader, and it is a layer of its own for two reasons that are the
# same reason twice: a render source runs once per case directory and a frame cost has no case
# directory, and the render layer carries a SANITISER, which multiplies a frame time by an
# instrument. A measurement whose subject is a duration cannot be taken through a bounds checker and
# be reported as the shipping frame -- so the split is what keeps the sanitised run from ever
# looking like the timed one.
LayerIncludes() {
  case "$1" in
    outshine/unit/core) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    outshine/unit/data) printf '%s' "-Isrc/core -Isrc/data" ;;
    outshine/unit/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    outshine/unit/ui) printf '%s' "-Isrc/core -Isrc/ui" ;;
    outshine/unit/scenario) printf '%s' "-Isrc/core -Isrc/scenario" ;;
    outshine/unit/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    outshine/unit/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    outshine/unit/world) printf '%s' "-Isrc/core -Isrc/data -Isrc/world -Isrc/world/tiles" ;;
    outshine/unit/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    outshine/unit/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    outshine/unit/render/stages) printf '%s' "-Isrc/core -Isrc/render/stages" ;;
    outshine/unit/clients) printf '%s' "-Isrc/clients" ;;
    outshine/harness) printf '%s' "-Isrc/core" ;;
    harness/khronos/glTF | harness/outshine/render) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients" ;;
    harness/wpt/css) printf '%s' "-Isrc/core -Isrc/ui" ;;
    outshine/frame) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/ui" ;;
    outshine/shader) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render -Isrc/render/draw -Isrc/render/plan -Isrc/render/stages" ;;
    # THE BROWSER READS A CASE THE WAY THE RUNNER DOES, so it compiles the runner's own reader and
    # sees exactly the layers that reader sees -- one set, not a second one that could drift.
    viewer) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients" ;;
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
# layer takes the renderer's own toolchain: C++20, SDL3 for the device and SDL3_image for the image
# boundary.
LayerToolchain() {
  case "$1" in
    harness/khronos/glTF | harness/outshine/render | outshine/frame | viewer) printf '%s' "-std=c++20 $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    outshine/shader) printf '%s' "-std=c++20 $(pkg-config --cflags sdl3)" ;;
    outshine/unit/clients) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3-image)" ;;
    *) printf '%s' "$CXXSTD" ;;
  esac
}
# THE SANITISERS ARE AN INSTRUMENT AND A LAYER DECLARES WHETHER IT WANTS ONE. `render` and `shader`
# do, and they are the layers that talk to a GPU API with hand-managed buffers, transfer copies and
# mapped ranges -- the exact places a use-after-free lives. -fno-sanitize-recover is what makes
# UndefinedBehaviorSanitizer a verdict rather than a log: without it a signed overflow prints and the
# run exits 0. THE INSTRUMENTED OBJECTS LIVE IN THEIR OWN DIRECTORY, so an instrumented object can
# never be linked into the binary the comparison is taken from, and the instrument appears in the
# test's own id so a sanitised run can never be read as a shipping one.
LayerSanitiser() {
  case "$1" in
    harness/khronos/glTF | harness/outshine/render | outshine/shader) printf '%s' "-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1" ;;
    *) printf '%s' "" ;;
  esac
}

# WHICH LAYERS GET THE API-CONTRACT ARM, and `frame` is excluded for the reason the sanitiser
# excludes it: a duration measured through an instrument is not the shipping frame (board:1123). The
# arm brings a device up with the driver's own validation on, over the SAME cases and the same
# assets -- it differs from the plain arm only in what it is allowed to NOTICE.
#
# IT IS ALSO WHAT PROVES board:1121's FOURTH CLAUSE: that every pipeline writing a target the plan
# pruned has an output set matching its pass. Three green clauses shipped undefined behaviour without
# it -- the target was neither allocated nor attached, the two plans differed, no case moved, and
# `SubjectDraw` still declared two colour outputs into a pass with one attachment.
LayerValidation() {
  case "$1" in
    harness/khronos/glTF | harness/outshine/render | outshine/shader) printf '%s' "-DOUTSHINE_GPU_VALIDATION=1" ;;
    *) printf '%s' "" ;;
  esac
}

LayerLink() {
  case "$1" in
    # zlib, for the oracle's EXR (board:1119). It is already in these processes through SDL3_image ->
    # libpng, so naming it links what the host already provides rather than adding a dependency.
    harness/khronos/glTF | harness/outshine/render | outshine/frame | viewer) printf '%s' "$(pkg-config --libs sdl3) $(pkg-config --libs sdl3-image) -lz" ;;
    outshine/harness) printf '%s' "-lz" ;;
    outshine/shader) printf '%s' "$(pkg-config --libs sdl3)" ;;
    outshine/unit/clients) printf '%s' "$(pkg-config --libs sdl3-image)" ;;
    *) printf '%s' "" ;;
  esac
}

# WHAT A TEST OF THIS LAYER LINKS. Layer archives are a later step; until then the harness compiles
# the same source groups the Makefile does, each with its own directory's include set.
#
# `outshine/unit/world` LINKS NOTHING and that is a limit, not a choice: src/world/tiles decodes terrarium PNG
# and imagery JPEG through SDL3_image, whose flags come from pkg-config -- and this harness's whole
# dependency set is the shell, the compiler and the clock. A test that needs to RUN world code is
# what pays to widen that.
#
# `outshine/unit/render/stages` LINKS NOTHING EITHER, and there it is a property of the subject rather than a
# limit: a shading model is a header of pure functions, so the white furnace integrates it with no
# object to link and no device to bring up.
#
# `shader` LINKS ONE FILE. src/render/Readback.cpp is the GPU->CPU transfer the library already owns,
# and a test that spelled its own download and its own map would be comparing two halves of a
# shading model through a third thing nothing else uses.
#
# `outshine/unit/clients` PAID IT, for one file. src/clients/Image.cpp IS the SDL3_image boundary -- the decode
# a glTF base-colour texture arrives through and the encode a render case's pictures leave through --
# so a test of it that stood in for the library would be testing the stand-in. The widening is one
# layer and one source and it changes nothing for the others: every remaining unit layer still links
# nothing at all.
LayerGroups() {
  case "$1" in
    outshine/unit/core) printf '%s' "src/core src/core/io" ;;
    outshine/unit/data) printf '%s' "src/core src/core/io src/data" ;;
    outshine/unit/gltf) printf '%s' "src/core src/gltf" ;;
    outshine/unit/ui) printf '%s' "src/core src/ui" ;;
    harness/wpt/css) printf '%s' "src/core/Json.cpp src/ui" ;;
    outshine/unit/scenario) printf '%s' "src/core src/scenario" ;;
    outshine/unit/generators) printf '%s' "src/core src/generators" ;;
    outshine/unit/generators/draw) printf '%s' "src/core src/generators src/generators/draw" ;;
    outshine/unit/world) printf '%s' "" ;;
    outshine/unit/render/plan) printf '%s' "src/core src/core/io src/render/plan" ;;
    outshine/unit/render/draw) printf '%s' "src/core src/core/io src/render/draw" ;;
    outshine/unit/render/stages) printf '%s' "" ;;
    outshine/unit/clients) printf '%s' "src/clients/Image.cpp" ;;
    outshine/harness) printf '%s' "src/core/Sha256.cpp src/core/Json.cpp" ;;
    harness/khronos/glTF | harness/outshine/render | outshine/frame | viewer) printf '%s' "src/core src/core/io src/gltf src/render/plan src/render/draw src/render src/render/stages src/ui src/clients/GltfStudio.cpp src/clients/Image.cpp" ;;
    # `outshine/shader` COMPILES THE RENDERER'S OWN STAGES AS WELL AS ITS OWN TWINS (board:1207). A
    # twin proves an arithmetic; only the unit's OWN text proves that the driver accepts it, and a
    # `shadeRow` call left one argument short survived a green library, a green unit tree and a green
    # shader suite because nothing in this layer compiled `SubjectDraw`'s source. It is still no
    # asset, no camera and no oracle -- which is what this layer is -- and it is now a device against
    # the shader the engine actually ships.
    outshine/shader) printf '%s' "src/core src/core/io src/render/plan src/render/draw src/render src/render/stages" ;;
    *) return 1 ;;
  esac
}

# WHAT ONE TEST SOURCE IS RUN OVER. Every layer but `render` runs its binary once, with no argument.
# A RENDER CASE IS A DIRECTORY (board:0083): the runner is built once and invoked
# once per case directory with the directory as its argument, so what is shared is the CODE and never
# the process -- still one process and one real verdict per case, and a crash in case 137 fails case
# 137 and nothing else.
LayerCases() {
  case "$1" in
    harness/khronos/glTF) find test/khronos/glTF -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/outshine/render) find test/outshine/render -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/wpt/css) find test/wpt/css -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    *) printf '%s' "" ;;
  esac
}

# WHAT IS UNDER test/ AND IS NOT THE HARNESS'S. None of these has a reporter or a verdict, and each
# says who does build it: the Makefile, the layer's own refusal test, or the offline preparer. There
# is no default arm here either.
#
# THE PREPARER'S OWN PROGRAM IS THE THIRD ANSWER AND IT IS NOT A LOOPHOLE. A generated part's bytes
# must exist before Blender opens, a generator is C++, and the alternative -- growing the part in the
# preparer's Python -- would score a subject this engine does not draw. So the preparer builds and
# runs it (test/corpus/prep/grown.py), the harness never touches it, and what the emit path
# guarantees is held by a test that does run: outshine/unit/gltf/AProducedSubjectIsTheOneItStated.
NotTheHarnesses() {
  case "$1" in
    harness/shared | harness/khronos/glTF | harness/outshine/render) printf '%s' "the harness's own clock and its prune, run by this script and judged by nobody" ;;
    harness/shared/render) printf '%s' "the render scoring instrument, compiled into each corpus's own harness" ;;
    outshine/host) printf '%s' "host implementations of what the library declares, compiled into the library" ;;
    outshine/unit/compile | outshine/unit/compile/*) printf '%s' "a compile subject, judged by the layer's own refusal test, never linked" ;;
    harness/khronos/glTF/prepare | harness/outshine/render/prepare | harness/wpt/css/prepare) printf '%s' "how a corpus is obtained, run by test/harness/shared/corpus/prepare.py and never by this script" ;;
    harness/shared/corpus | harness/shared/corpus/*) printf '%s' "the offline preparer's own, compiled and run by test/harness/shared/corpus/prepare.py" ;;
    *) return 1 ;;
  esac
}

# WHAT A SUITE COMPILES BESIDE ITS OWN HARNESS. A corpus is a folder with its own runner, and the
# measurement that runner performs is shared -- a case is decided the same way whoever authored the
# asset. So the scorer is one file compiled into each harness rather than one binary behind a flag.
LayerExtraSources() {
  case "$1" in
    harness/khronos/glTF | harness/outshine/render | viewer) printf '%s' "test/harness/shared/render/Parity.cpp" ;;
    *) printf '%s' "" ;;
  esac
}

# A SOURCE COMPILES WITH ITS OWN DIRECTORY'S INCLUDE SET, never with the test's: that is what makes
# the layering the build rather than a rule, and it is the same set the Makefile hands each group.
GroupIncludes() {
  case "$1" in
    src/core | src/core/io | src/core/Sha256.cpp | src/core/Json.cpp) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    src/data) printf '%s' "-Isrc/core -Isrc/data" ;;
    src/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    src/ui) printf '%s' "-Isrc/core -Isrc/ui" ;;
    src/scenario) printf '%s' "-Isrc/core -Isrc/scenario" ;;
    src/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    src/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    src/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    src/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    src/render | src/render/stages | src/render/Readback.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages $(pkg-config --cflags sdl3)" ;;
    src/clients/GltfStudio.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients $(pkg-config --cflags sdl3)" ;;
    src/clients/Image.cpp) printf '%s' "-Isrc/clients $(pkg-config --cflags sdl3-image)" ;;
    *) return 1 ;;
  esac
}

# A SOURCE THAT NEEDS MORE THAN THE HOUSE STANDARD SAYS SO ONCE. The renderer is C++20 and speaks to
# SDL_GPU; nothing else in the tree is either.
GroupToolchain() {
  case "$1" in
    src/render | src/render/stages | src/render/Readback.cpp | src/clients/GltfStudio.cpp) LayerToolchain render ;;
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
      $CXX "$unit" $groupStd $OPT $WARN $SAN $EXTRA_DEFINES -MMD -MP $groupIncludes -c -o "$unitObject" || return 1
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

mkdir -p "$BUILD/obj" "$BUILD/obj-sanitised" "$BUILD/obj-validated" "$BUILD/log" || Die "cannot write under $BUILD"
SAN=""
$CXX test/harness/shared/Millis.cpp $CXXSTD $OPT $WARN -o "$BUILD/millis" || Die "the clock did not build"
Now() { "$BUILD/millis"; }

# THE PRUNE, BESIDE THE CLOCK AND FOR THE SAME REASON (board:1181): the runner owns a case's
# lifecycle, so the runner is what declines to keep a second copy of it -- and neither program holds a
# verdict, which is why both are here rather than in a layer. Its two library sources go through
# BuildGroup like every other, so they compile with src/core's include set and not with this one.
OBJECTS=""
BuildGroup src/core/Json.cpp || Die "the prune's reader did not build"
# shellcheck disable=SC2086
$CXX test/harness/shared/Prune.cpp $OBJECTS $CXXSTD $OPT $WARN -Itest/harness/shared -Itest/harness/shared/render -Isrc/core -o "$BUILD/prune" ||
  Die "the prune did not build"
PRUNE_MARKER=$BUILD/prune.marker

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

# A RENDER CASE IS A DIRECTORY, SO NAMING ONE SELECTS IT (board:1410). Without this the smallest
# thing that can be run is a whole declarative suite -- 45 cases to see one number move, and the
# corpus is 148 -- which is a tax on exactly the iteration the corpus exists to make cheap.
#
# THE LAYER IS DERIVED BY ASKING EACH LAYER'S OWN ENUMERATION whether it holds the path, never by a
# second table beside `LayerCases`: a mapping written twice is a mapping that can disagree with
# itself, and this one would disagree silently by running the wrong suite over no cases at all.
#
# THE TRAILER STILL DECIDES. A filtered run reports the count it actually ran, so `1 tests: 1 PASS`
# cannot be quoted as a suite -- which is the same protection the trailer already gives every run.
CASE=
if [ -n "$SUITE" ] && [ -f "test/$SUITE/manifest.json" ]; then
  for candidate in $(for one in $TESTS; do dirname "${one#test/}"; done | sort -u); do
    if LayerCases "$candidate" | grep -qxF "test/$SUITE"; then
      CASE=test/$SUITE
      SUITE=$candidate
      break
    fi
  done
  [ -n "$CASE" ] ||
    Die "test/$SUITE carries a manifest and no declared layer enumerates it -- add it to LayerCases, or name the suite instead"
fi

if [ -n "$SUITE" ]; then
  selected=""
  for candidate in $TESTS; do
    case "${candidate#test/}" in "$SUITE"/*) selected="$selected $candidate" ;; esac
  done
  [ -n "$selected" ] ||
    Die "no declared suite under test/$SUITE -- $(find test -name '*.cpp' -exec dirname {} \; | sed 's|^test/||' | sort -u | tr '\n' ' ')"
  TESTS=$selected
  if [ -n "$CASE" ]; then
    printf 'run.sh: %s only, under test/%s\n' "$CASE" "$SUITE"
  else
    printf 'run.sh: %s only\n' "test/$SUITE"
  fi
fi

started=$(Now)
passed=0
# THE TWO COUNTS THE FINISH LINE NAMES, SUMMED HERE AND NOWHERE ELSE (board:1208). Every render case
# already prints `KHRONOS-CRITERION` and `PICTURE-BOUND` -- the partition is a field of the metric so
# that no reporter has to match names -- and they lived in one log per case and in no total. They are
# accumulated FROM THIS RUN's logs as each case finishes, never scanned off disk afterwards: a count
# gathered from whatever survived the last run is board:1181's hazard in a new place.
criterionMet=0
criterionRed=0
pictureWithin=0
pictureOutside=0
pictureUnenforced=0
grownCriterionMet=0
grownCriterionRed=0
grownPictureWithin=0
grownPictureOutside=0
grownPictureUnenforced=0
uiInside=0
uiOutside=0
uiHeld=0
uiRed=0
failed=0
timedout=0
signalled=0
unbuilt=0
skipped=0
unprepared=0
undeclaredSkips=0
inverted=0

peakKib=0
endKib=0
prunedCases=0
prunedFiles=0
prunedKib=0
stayedFiles=0

# WHAT THE SUITE COSTS ON DISK RIGHT NOW. One process per sample: a per-file walk would be a thousand
# more of them for a number this is quoted in megabytes.
PreparedCase() { printf '%s/%s' "$PREPARED" "$(printf '%s' "$1" | tr / -)"; }

SuiteKib() {
  # shellcheck disable=SC2046
  set -- $(du -sk "$PREPARED" 2>/dev/null)
  printf '%s' "${1:-0}"
}

# THE PEAK IS SAMPLED WHERE THE PEAK IS: a case that has been rendered, judged by every arm, and not
# yet pruned. A run-wide average would say nothing about a quantity whose whole point is its maximum.
SampleSuite() {
  sampled=$(SuiteKib)
  [ "$sampled" -gt "$peakKib" ] && peakKib=$sampled
  return 0
}

# ALWAYS, PER CASE, PASS OR FAIL (board:1181). The store holds every oracle product of every case and
# our own dumps are reproduced by re-running the case, so this declines to keep a second copy rather
# than deleting anything. What it cannot prove it leaves standing, and the case's own prune log names
# the proof that refused and the command that brings the file back.
PruneCase() {
  pruneCase=$1
  prunePrepared=$2
  pruneLog=$BUILD/log/$(printf '%s' "${pruneCase#test/}" | tr / -)-prune.log
  if ! pruneSummary=$("$BUILD/prune" "$prunePrepared" "$PRUNE_MARKER" 2>"$pruneLog"); then
    printf 'run.sh: %s was not pruned -- %s\n' "${pruneCase#test/}" "$pruneLog" >&2
    return 0
  fi
  # shellcheck disable=SC2086
  set -- $pruneSummary
  { [ $# -eq 5 ] && [ "$1" = PRUNE ]; } ||
    Die "the prune printed a summary it cannot have written -- $pruneLog"
  prunedCases=$((prunedCases + 1))
  prunedFiles=$((prunedFiles + $2))
  prunedKib=$((prunedKib + $3 / 1024))
  stayedFiles=$((stayedFiles + $4))
  printf '%-7s %-46s %6s MB  %s\n' PRUNE "${pruneCase#test/}" "$(($3 / 1048576))" "$pruneLog"
  [ "$4" -eq 0 ] ||
    printf 'run.sh: %s kept %s file(s) whose producer it could not prove -- %s\n' \
      "${pruneCase#test/}" "$4" "$pruneLog" >&2
  return 0
}

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
  CountTheTwo "$judgeId" "$log"
  Record "$judgeId" "$(( $(Now) - before ))"
}

# ONE CASE CONTRIBUTES ONE VOTE, NOT THREE (board:1208). A case runs plain, sanitised and validated,
# and the two counts are about the PICTURE -- so only the arm with no `~` in its id votes, and the
# other two are instruments about that same picture rather than two more pictures.
CountTheTwo() {
  case "$1" in *'~'*) return 0 ;; esac
  [ -f "$2" ] || return 0
  # THE CORPUS IS PART OF THE POPULATION AND THE LABEL SAYS SO. The first version of this counter
  # summed both corpora under the word `khronos` and published `38 met of 44` -- 33 Khronos cases and
  # 11 this engine grows itself, under a name that claimed only the first. A count whose label names a
  # narrower population than it draws from is board:0089's own warning arriving in the reporter.
  subset=$(sed -n 's/^UI-SUBSET //p' "$2" | head -1)
  layout=$(sed -n 's/^UI-LAYOUT //p' "$2" | head -1)
  case "$subset" in
    inside) uiInside=$((uiInside + 1)) ;;
    outside) uiOutside=$((uiOutside + 1)) ;;
  esac
  case "$layout" in
    held) uiHeld=$((uiHeld + 1)) ;;
    red) uiRed=$((uiRed + 1)) ;;
  esac
  criterion=$(sed -n 's/^KHRONOS-CRITERION //p' "$2" | head -1)
  picture=$(sed -n 's/^PICTURE-BOUND //p' "$2" | head -1)
  case "$1" in
    khronos/*)
      case "$criterion" in
        met) criterionMet=$((criterionMet + 1)) ;;
        red) criterionRed=$((criterionRed + 1)) ;;
      esac
      case "$picture" in
        within) pictureWithin=$((pictureWithin + 1)) ;;
        outside) pictureOutside=$((pictureOutside + 1)) ;;
        not-enforced) pictureUnenforced=$((pictureUnenforced + 1)) ;;
      esac
      ;;
    *)
      case "$criterion" in
        met) grownCriterionMet=$((grownCriterionMet + 1)) ;;
        red) grownCriterionRed=$((grownCriterionRed + 1)) ;;
      esac
      case "$picture" in
        within) grownPictureWithin=$((grownPictureWithin + 1)) ;;
        outside) grownPictureOutside=$((grownPictureOutside + 1)) ;;
        not-enforced) grownPictureUnenforced=$((grownPictureUnenforced + 1)) ;;
      esac
      ;;
  esac
}

# EVERY ARM OF ONE INVOCATION, BACK TO BACK. The loop is CASE-OUTER and ARM-INNER (board:1181): a
# case runs plain, sanitised and validated in turn and is then pruned, so a case's inputs have to
# survive until the last of its own arms rather than until the last arm of the whole suite. Same
# binaries, same set, same verdicts, different order -- all three are built before any case runs, so
# the cost of the inversion is nothing at all.
#
# A LAYER WITH NO CASES RUNS THE SAME THREE ARMS WITH NO ARGUMENT, which is why both live here once:
# the case-less arm used to return early and a sanitiser such a layer declared was never applied to
# anything.
#   $1 the id stem   $2 the argument, or empty
# WHETHER A TEST BINARY IS ALREADY THE ONE THIS COMMAND WOULD PRODUCE (board:1371).
#
# EVERY TEST SOURCE WAS COMPILED AND LINKED ON EVERY RUN, THREE ARMS EACH. [MEASURED] on the shader
# suite: 24 tests summing 7.7 s of their own time inside a 21.7 s warm run -- 14 s, nearly two thirds,
# spent rebuilding what had not changed. Metal is NOT the cost and that was measured too: a device plus
# eight shader variants is 0.13 s, warm-cached by macOS, and a test with no device starts in 0.00 s.
#
# TWO THINGS DECIDE IT AND BOTH ARE NECESSARY. The `.d` file `-MMD` writes lists every header the
# compile actually read, which is the same mechanism the library objects already use; and the COMMAND
# is written beside the binary, because `$compileDefine` splices a layer's own include set into the
# binary and a changed include set must rebuild even when no file moved.
#   $1 the binary   $2 the exact command that would produce it
#   $3.. every source compiled into it, which its own `.d` DOES NOT NECESSARILY NAME (board:1446).
# `-MMD` with several inputs and one `-o` writes ONE dependency file and fills it from the LAST
# translation unit only. [MEASURED] `viewer`'s binary listed `test/harness/shared/render/Parity.cpp`
# and its headers, and did NOT list `test/viewer/EveryCaseTheTreeDeclaresConfigures.cpp` -- the test's
# own source. Touching that source rebuilt nothing and the run reported a verdict from a binary
# compiled before the edit, which is the same phantom board:1403 caught one layer down. Every layer
# carrying extra sources has this shape, and that is both render corpora.
Fresh() {
  freshBinary=$1
  freshCommand=$2
  shift 2
  [ -f "$freshBinary" ] || return 1
  [ -f "$freshBinary.cmd" ] || return 1
  [ "$(cat "$freshBinary.cmd")" = "$freshCommand" ] || return 1
  [ -f "$freshBinary.d" ] || return 1
  for freshSource in "$@"; do
    [ -f "$freshSource" ] || return 1
    [ "$freshSource" -nt "$freshBinary" ] && return 1
  done
  # Every prerequisite the compiler recorded, minus the make syntax around them.
  for freshNeed in $(tr '\\' ' ' <"$freshBinary.d" | tr ':' ' ' | tr -s ' \n' ' '); do
    case "$freshNeed" in *.o|*.cpp|*.h|*.hpp) ;; *) continue ;; esac
    [ -f "$freshNeed" ] || return 1
    [ "$freshNeed" -nt "$freshBinary" ] && return 1
  done
  # AND EVERY OBJECT IT LINKS, WHICH THE COMPILER'S OWN LIST DOES NOT CARRY (board:1403). `-MMD` records the
  # headers a TRANSLATION UNIT read; the library's objects are LINK inputs and appear in no `.d` at
  # all. [MEASURED] a change to `src/gltf/Document.cpp` left every unit-test binary untouched and the
  # suite reported green -- against a library the binary was not built with, which is the one failure
  # a freshness check exists to make impossible. The `*.o` arm above was written for these and never
  # saw one.
  for freshObject in $OBJECTS; do
    [ -f "$freshObject" ] || return 1
    [ "$freshObject" -nt "$freshBinary" ] && return 1
  done
  return 0
}

JudgeArms() {
  armStem=$1
  armArgument=$2
  Judge "$armStem" "$plainBinary" "$armArgument"
  JudgeInstrument "$armStem" no
  if [ -n "$sanitisedBinary" ]; then
    # DETECT_STACK_USE_AFTER_RETURN IS PART OF THE INSTRUMENT: a build that carries the
    # instrumentation still reports nothing about it without this line.
    ASAN_OPTIONS=detect_stack_use_after_return=1
    UBSAN_OPTIONS=print_stacktrace=1
    export ASAN_OPTIONS UBSAN_OPTIONS
    Judge "$armStem~sanitised" "$sanitisedBinary" "$armArgument"
    JudgeInstrument "$armStem~sanitised" yes
    unset ASAN_OPTIONS UBSAN_OPTIONS
  fi
  if [ -n "$validatedBinary" ]; then
    validatedRan=yes
    Judge "$armStem~validated" "$validatedBinary" "$armArgument"
    JudgeInstrument "$armStem~validated" yes
  fi
  return 0
}

# WHAT THE INSTRUMENT ITSELF SAID, which the trailer cannot carry: AddressSanitizer and
# UndefinedBehaviorSanitizer write to the log and the process can still exit 0 over a clean trailer.
#   $1 the id it is reported under   $2 yes if an instrument is in the path
JudgeInstrument() {
  [ "$2" = yes ] || return 0
  grep -qE "AddressSanitizer|runtime error:" "$log" || return 0
  printf 'run.sh: %s -- the run finished and the sanitiser spoke, %s\n' "$1" "$log" >&2
  failed=$((failed + 1))
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
      printf 'run.sh: %s has no prepared input -- run test/harness/shared/corpus/prepare.py\n' "$recordId" >&2
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
  # ALL THREE NAMES DERIVED FROM THE TEST'S ID, never one from another: `RunWithTimeout` assigns
  # `binary` as a shell global, so `$binary.validated` once named an arm `...sanitised.validated` --
  # a file name that said the run carried a sanitiser when it did not, which is the archive defect
  # this tree has already paid for once (board:1123).
  plainBinary=$BUILD/$(printf '%s' "$id" | tr / -)
  sanitisedBinary=""
  validatedBinary=""

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
    buildCommand="$CXX $testSource $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $includes $compileDefine $linkage"
    if Fresh "$plainBinary" "$buildCommand" $testSource $(LayerExtraSources "$layer"); then :; else
      $CXX "$testSource" $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN -Itest/harness/shared -Itest/harness/shared/render $includes "$compileDefine" $linkage -MMD -MP -MF "$plainBinary.d" -o "$plainBinary" >>"$log" 2>&1 && printf '%s' "$buildCommand" >"$plainBinary.cmd" || built=no
    fi
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

  # EVERY ARM IS BUILT BEFORE ANY CASE RUNS. That is what the inversion costs and it is nothing: the
  # three binaries were always built before the second and third arms ran, and building them here
  # instead is what lets one case be judged three times and then pruned.
  sanitiser=$(LayerSanitiser "$layer")
  if [ -n "$sanitiser" ]; then
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
    sanitisedBinary=$plainBinary.sanitised
    if [ "$built" = yes ]; then
      # shellcheck disable=SC2086
      buildCommand="$CXX $testSource $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $SAN $includes $compileDefine $linkage"
    if Fresh "$sanitisedBinary" "$buildCommand" $testSource $(LayerExtraSources "$layer"); then :; else
      $CXX "$testSource" $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $SAN -Itest/harness/shared -Itest/harness/shared/render $includes "$compileDefine" $linkage -MMD -MP -MF "$sanitisedBinary.d" -o "$sanitisedBinary" >>"$sanitisedLog" 2>&1 && printf '%s' "$buildCommand" >"$sanitisedBinary.cmd" || built=no
    fi
    fi
    OBJDIR=$BUILD/obj
    SAN=""
    if [ "$built" = no ]; then
      failures=0
      skips=0
      verdict=BUILD
      Record "$id~sanitised" "$(( $(Now) - before ))"
      sanitisedBinary=""
    fi
  fi

  # THE API-CONTRACT ARM (board:1123). Same source, same cases, same assets; the device is created
  # with the driver's validation enabled, so a pipeline whose output set disagrees with its pass
  # aborts here instead of rendering correctly and being undefined. A layer whose sanitised arm did
  # not build does not reach it, which is what the returns it replaced already did.
  validation=$(LayerValidation "$layer")
  if [ -n "$validation" ] && { [ -z "$sanitiser" ] || [ -n "$sanitisedBinary" ]; }; then
    before=$(Now)
    OBJDIR=$BUILD/obj-validated
    EXTRA_DEFINES=$validation
    OBJECTS=""
    built=yes
    validatedLog=$BUILD/log/$(printf '%s' "$id" | tr / -)-validated.log
    : >"$validatedLog"
    for group in $groups; do
      BuildGroup "$group" >>"$validatedLog" 2>&1 || built=no
    done
    validatedBinary=$BUILD/$(printf '%s' "$id" | tr / -).validated
    if [ "$built" = yes ]; then
      # shellcheck disable=SC2086
      buildCommand="$CXX $testSource $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $validation $includes $compileDefine $linkage"
    if Fresh "$validatedBinary" "$buildCommand" $testSource $(LayerExtraSources "$layer"); then :; else
      $CXX "$testSource" $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $validation -Itest/harness/shared -Itest/harness/shared/render $includes "$compileDefine" $linkage -MMD -MP -MF "$validatedBinary.d" -o "$validatedBinary" >>"$validatedLog" 2>&1 && printf '%s' "$buildCommand" >"$validatedBinary.cmd" || built=no
    fi
    fi
    OBJDIR=$BUILD/obj
    EXTRA_DEFINES=""
    if [ "$built" = no ]; then
      failures=0
      skips=0
      verdict=BUILD
      Record "$id~validated" "$(( $(Now) - before ))"
      validatedBinary=""
    fi
  fi

  # A DECLARATIVE SUITE WITH NO DECLARATION IS THE VACUOUS GATE IN ITS PUREST FORM: a runner that
  # runs over nothing and reports nothing, green. The enumeration is the tracked manifests, so an
  # empty set here means the suite itself is empty, and the runner is then invoked once with no
  # argument -- which is a refusal from the runner and never a silent pass.
  cases=$(LayerCases "$layer")
  if [ -z "$cases" ]; then
    JudgeArms "$id" ""
    continue
  fi

  # A CASE DIRECTORY MAY CARRY A SPACE AND ONE DOES (board:1228). `Box With Spaces` is a Khronos
  # sample whose whole point is that its name and its files carry spaces, and an unquoted word split
  # here turned it into three case paths that do not exist -- reported as UNPREPARED rather than as a
  # failure, so the model silently had no case at all while its manifest sat in the tree. Splitting on
  # newline alone is what the enumeration above actually produces.
  oldIfs=$IFS
  IFS='
'
  for oneCase in $cases; do
    IFS=$oldIfs
    if [ -n "$CASE" ] && [ "$oneCase" != "$CASE" ]; then IFS='
'; continue; fi
    # THE MARKER IS WHAT "THIS RUN WROTE IT" IS MEASURED AGAINST, and it lives outside the case
    # directory: a marker inside one would be a file the prune then had to have an opinion about.
    : >"$PRUNE_MARKER"
    preparedCase=$(PreparedCase "$oneCase")
    JudgeArms "${oneCase#test/}" "$preparedCase"
    SampleSuite
    PruneCase "$oneCase" "$preparedCase"
    IFS='
'
  done
  IFS=$oldIfs
done
endKib=$(SuiteKib)

total=$((passed + failed + timedout + signalled + unbuilt + skipped + unprepared))
printf '%s tests: %s PASS  %s FAIL  %s TIMEOUT  %s SIGNAL  %s BUILD  %s SKIP  %s UNPREPARED  in %s ms\n' \
  "$total" "$passed" "$failed" "$timedout" "$signalled" "$unbuilt" "$skipped" "$unprepared" \
  "$(( $(Now) - started ))"
# THE TWO COUNTS, SIDE BY SIDE AND NEITHER QUOTABLE AS THE OTHER (board:1208). `criteria met` counts
# FEATURES and does not fall because our picture is not the reference's; the picture bound counts
# PICTURES. `not-enforced` is its own column rather than folded into either, because a case nobody can
# count either way would otherwise be counted as a pass. Printed only where a case reported one, so a
# run of the unit tree says nothing about a corpus it never touched.
[ $((criterionMet + criterionRed)) -gt 0 ] &&
  printf 'khronos: criteria %s met of %s   picture bound %s within, %s outside, %s not-enforced of %s\n' \
    "$criterionMet" "$((criterionMet + criterionRed))" \
    "$pictureWithin" "$pictureOutside" "$pictureUnenforced" \
    "$((pictureWithin + pictureOutside + pictureUnenforced))"
[ $((grownCriterionMet + grownCriterionRed)) -gt 0 ] &&
  printf 'grown:   criteria %s met of %s   picture bound %s within, %s outside, %s not-enforced of %s\n' \
    "$grownCriterionMet" "$((grownCriterionMet + grownCriterionRed))" \
    "$grownPictureWithin" "$grownPictureOutside" "$grownPictureUnenforced" \
    "$((grownPictureWithin + grownPictureOutside + grownPictureUnenforced))"
# THE UI SUITE'S OWN PAIR, AND IT IS THE SAME SHAPE FOR THE SAME REASON (board:1444). `inside the
# subset` counts DECLARATIONS this engine claims to be able to express; `held` counts the ones whose
# every stated box landed. Neither stands for the other, and the first is the one that would improve
# by shrinking -- a suite reporting only `held` gets greener the less of the corpus it attempts.
[ $((uiInside + uiOutside)) -gt 0 ] &&
  printf 'wpt:     subset %s inside of %s   layout %s held, %s red of %s\n' \
    "$uiInside" "$((uiInside + uiOutside))" \
    "$uiHeld" "$uiRed" "$((uiHeld + uiRed))"
# WHAT THE API-CONTRACT ARM DOES NOT COVER, printed where its results are, because a green
# validation arm is not a correctness claim and a later round must not read it as one (board:1123).
[ "$validatedRan" = yes ] && printf '%s\n' \
  "~validated is an API-CONTRACT arm: it says the pipelines, passes and resources agree with the driver, and NOTHING about whether the picture is right -- that is render/'s domain and its oracle's"
[ "$inverted" -gt 0 ] && printf 'expect-fail inverted: %s\n' "$EXPECT_FAIL"

# THE HIGH-WATER MARK, AS A NUMBER (board:1181), because "size doesn't grow" is only checkable
# against one and a later round that breaks this must be caught by the number rather than by somebody
# noticing the disk. The peak is sampled at each case's own prune, which is where a case is at its
# largest: rendered, judged by every arm, and not yet pruned. The runner does not MATERIALISE a case
# -- that is the preparer's, and it is why the peak is bounded by what the preparer left standing
# when the run began, not by any one case.
[ "$prunedCases" -gt 0 ] && printf \
  'test corpora: peak %s MB, %s MB after the last prune -- %s cases pruned, %s files and %s MB declined, %s file(s) left standing (each case: %s/*-prune.log)\n' \
  "$((peakKib / 1024))" "$((endKib / 1024))" "$prunedCases" "$prunedFiles" "$((prunedKib / 1024))" \
  "$stayedFiles" "$BUILD/log"

# A SKIP IS RED UNLESS IT WAS DECLARED ON THE COMMAND LINE, AND AN UNPREPARED CASE IS RED WITH NO
# WAY TO DECLARE IT AWAY. A silent skip is the defect class this repository keeps finding, wearing a
# harness's hat; an unprepared corpus wearing a skip's hat would be the same defect one level up --
# a tier that skips when its inputs are absent cannot be told from a tier that passed having tested
# nothing.
red=$((failed + timedout + signalled + unbuilt + undeclaredSkips + unprepared))
[ "$red" -eq 0 ] || exit 1
exit 0
