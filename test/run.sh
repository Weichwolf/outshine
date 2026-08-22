#!/bin/sh

set -u
set -m
AUDIT=0
AUDITLINK=0

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 2

BUILD=${TMPDIR:-/tmp}
# the nest carries the checkout identity (board:1649): parallel checkouts get parallel
# nests, a worktree gate cannot sweep this one mid-run, and a collision is unspellable
NEST=$(printf %s "$ROOT" | shasum -a 256 | cut -c1-12)
BUILD=${BUILD%/}/outshine-tests.$NEST
export OUTSHINE_NEST="$BUILD"
PREPARED=${TMPDIR:-/tmp}
PREPARED=${PREPARED%/}/outshine-prepared
CXX=${CXX:-c++}
CXXSTD=-std=c++23
WARN="-Wall -Wextra -Wpedantic -Wshadow -Werror -Wno-unused-parameter"
OPT=-O2

TIMEOUT_S=120
ALLOWED_SKIPS=""
EXTRA_DEFINES=""
validatedRan=no

EXPECT_FAIL="harness/claims/ExpectFail:1"

Die() {
  printf 'run.sh: %s\n' "$*" >&2
  exit 2
}

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
SUITES=
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
    --library) LIBRARY=1; shift ;;
    --audit) AUDIT=1; shift ;;
    --audit-link) AUDITLINK=1; shift ;;
    -*) Die "unknown option '$1'" ;;
    *) SUITES="$SUITES ${1%/}"; SUITE=${1%/}; shift; continue ;;
  esac
done

LayerIncludes() {
  case "$1" in
    unit/core/io) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    unit/core) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    unit/actor/path) printf '%s' "-Isrc/actor/path" ;;
    unit/scene) printf '%s' "-Iinclude/outshine" ;;
    unit/sim) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/actor/body -Isrc/actor/mind -Isrc/scenario -Isrc/sim -Isrc/ground -Isrc/ground/tiles" ;;
    unit/actor/body) printf '%s' "-Isrc/actor/body" ;;
    unit/actor/mind) printf '%s' "-Isrc/actor/path -Isrc/actor/mind" ;;
    unit/data) printf '%s' "-Isrc/core -Isrc/data" ;;
    unit/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    unit/ui) printf '%s' "-Isrc/core -Isrc/ui" ;;
    unit/scenario) printf '%s' "-Iinclude -Isrc/core -Isrc/scenario" ;;
    unit/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    unit/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    unit/ground) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/ground -Isrc/ground/tiles" ;;
    unit/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    unit/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    unit/render/stages) printf '%s' "-Isrc/core -Isrc/render/stages" ;;
    unit/render) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages" ;;
    unit/clients) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/actor/path -Isrc/actor/body -Isrc/actor/mind -Isrc/scenario -Isrc/clients" ;;
    harness/claims) printf '%s' "-Isrc/core" ;;
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients" ;;
    harness/render/wpt/css) printf '%s' "-Isrc/core -Isrc/ui" ;;
    harness/render/test262/js) printf '%s' "-Isrc/core" ;;
    render/outshine/frame) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/ui" ;;
    render/outshine/shader) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render -Isrc/render/draw -Isrc/render/plan -Isrc/render/stages" ;;
    render/outshine/client) printf '%s' "-Iinclude" ;;
    render/outshine/world) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/data -Isrc/scenario -Isrc/generators -Isrc/generators/draw -Isrc/ground -Isrc/ground/tiles -Isrc/clients -Isrc/actor/path -Itools/host" ;;
    render/outshine/drive) printf '%s' "-Iinclude -Isrc/actor/path -Isrc/actor/body -Isrc/actor/mind" ;;
    render/outshine/scenario) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/ui -Itest/harness/shared" ;;
    tools/viewer) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/ui -Itools/viewer/parts" ;;
    tools/driver/stills | tools/driver/window) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/clients -Isrc/actor/path -Isrc/data -Isrc/gltf -Isrc/actor/body -Isrc/actor/mind -Isrc/render -Isrc/render/plan -Isrc/render/draw -Isrc/render/stages -Isrc/scenario -Isrc/ui -Isrc/ground -Isrc/ground/tiles -Itools/host -Isrc/sim" ;;
    tools/driver) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/clients -Isrc/actor/path -Isrc/data -Isrc/actor/body -Isrc/actor/mind -Isrc/gltf -Isrc/scenario -Isrc/ground -Isrc/ground/tiles -Itools/host -Isrc/sim" ;;
    *) return 1 ;;
  esac
}

LayerToolchain() {
  case "$1" in
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/frame | tools/viewer | render/outshine/scenario) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    render/outshine/client) printf '%s' "$CXXSTD" ;;
    render/outshine/drive) printf '%s' "$CXXSTD" ;;
    tools/driver) printf '%s' "$CXXSTD" ;;
    tools/driver/stills | tools/driver/window) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    render/outshine/shader) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3)" ;;
    unit/clients) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3-image)" ;;
    unit/render) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3)" ;;
    *) printf '%s' "$CXXSTD" ;;
  esac
}
LayerSanitiser() {
  case "$1" in
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/shader) printf '%s' "-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1" ;;
    *) printf '%s' "" ;;
  esac
}

LayerValidation() {
  case "$1" in
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/shader) printf '%s' "-DOUTSHINE_GPU_VALIDATION=1" ;;
    *) printf '%s' "" ;;
  esac
}

LayerLink() {
  case "$1" in
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/frame | tools/viewer | render/outshine/scenario | render/outshine/client) printf '%s' "$(pkg-config --libs sdl3) $(pkg-config --libs sdl3-image) -lz" ;;
    harness/claims) printf '%s' "-lz" ;;
    unit/core/io) printf '%s' "-lz" ;;
    render/outshine/world) printf '%s' "-lz -lcurl" ;;
    tools/driver) printf '%s' "-lz -lcurl" ;;
    tools/driver/stills | tools/driver/window) printf '%s' "$(pkg-config --libs sdl3) $(pkg-config --libs sdl3-image) -lz -lcurl" ;;
    render/outshine/shader) printf '%s' "$(pkg-config --libs sdl3) -lz" ;;
    unit/clients) printf '%s' "$(pkg-config --libs sdl3-image) -lz" ;;
    unit/render) printf '%s' "$(pkg-config --libs sdl3) -lz" ;;
    unit/core | unit/ground | unit/data | unit/render/plan | unit/render/draw | unit/sim | unit/actor/path) printf '%s' "-lz" ;;
    *) printf '%s' "" ;;
  esac
}

LayerGroups() {
  case "$1" in
    unit/core) printf '%s' "src/core src/core/io" ;;
    unit/core/io) printf '%s' "src/core src/core/io" ;;
    unit/actor/path) printf '%s' "src/actor/path" ;;
    unit/scene) printf '%s' "src/scene" ;;
    unit/sim) printf '%s' "src/core src/core/io src/actor/path src/data src/actor/body src/actor/mind src/scenario/ScenarioRead.cpp src/sim src/scene src/ground src/ground/tiles" ;;
    unit/actor/body) printf '%s' "src/actor/body" ;;
    unit/actor/mind) printf '%s' "src/actor/path src/actor/mind" ;;
    unit/data) printf '%s' "src/core src/core/io src/data" ;;
    unit/gltf) printf '%s' "src/core src/gltf" ;;
    unit/ui) printf '%s' "src/core src/ui" ;;
    harness/render/wpt/css) printf '%s' "src/core/Json.cpp src/ui" ;;
    harness/render/test262/js) printf '%s' "src/core/Json.cpp src/core/Script.cpp" ;;
    unit/scenario) printf '%s' "src/core src/scenario" ;;
    unit/generators) printf '%s' "src/core src/generators" ;;
    unit/generators/draw) printf '%s' "src/core src/generators src/generators/draw" ;;
    unit/ground) printf '%s' "src/core src/core/io src/actor/path src/data src/ground src/ground/tiles" ;;
    unit/render/plan) printf '%s' "src/core src/core/io src/render/plan" ;;
    unit/render/draw) printf '%s' "src/core src/core/io src/render/draw" ;;
    unit/render/stages) printf '%s' "" ;;
    unit/render) printf '%s' "src/core src/core/io src/render/plan src/render/draw src/render src/render/stages" ;;
    unit/clients) printf '%s' "src/core src/actor/path src/actor/body src/actor/mind src/scene src/clients/Image.cpp src/scenario/ScenarioRead.cpp src/clients/Assembly.cpp" ;;
    harness/claims) printf '%s' "src/core/Sha256.cpp src/core/Json.cpp" ;;
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/frame | tools/viewer | render/outshine/scenario | render/outshine/client) printf '%s' "src/core src/core/io src/gltf src/render/plan src/render/draw src/render src/render/stages src/scene src/ui src/clients/GltfStudio.cpp src/clients/Image.cpp src/clients/Surfaces.cpp src/clients/Live.cpp src/clients/Engine.cpp src/scenario/ScenarioRead.cpp src/clients/Assembly.cpp" ;;
    render/outshine/shader) printf '%s' "src/core src/core/io src/render/plan src/render/draw src/render src/render/stages" ;;
    tools/driver/stills | tools/driver/window) printf '%s' "src/core src/core/io src/gltf src/render/plan src/render/draw src/render src/render/stages src/ui src/actor/path src/actor/body src/actor/mind src/data src/ground/tiles src/ground src/clients/GltfStudio.cpp src/clients/Image.cpp src/clients/Surfaces.cpp src/clients/Live.cpp src/scenario/ScenarioRead.cpp src/sim src/scene src/clients/Assembly.cpp" ;;
    tools/driver) printf '%s' "src/core src/core/io src/gltf src/actor/path src/actor/body src/actor/mind src/data src/ground/tiles src/ground src/sim src/scene src/scenario/ScenarioRead.cpp src/clients/Assembly.cpp" ;;
    render/outshine/world) printf '%s' "src/core src/core/io src/data src/scenario src/generators src/generators/draw src/ground/tiles src/ground src/actor/path/Wayfinding.cpp src/clients/Sim.cpp src/clients/LogSinks.cpp src/clients/StreamTelemetry.cpp src/clients/EyeTelemetry.cpp src/clients/CsvTelemetry.cpp src/clients/Species.cpp src/clients/RegionForge.cpp src/clients/SceneWeather.cpp" ;;
    render/outshine/drive) printf '%s' "src/actor/path src/actor/body src/actor/mind" ;;
    *) return 1 ;;
  esac
}

LayerCases() {
  case "$1" in
    harness/render/khronos/glTF) find test/render/khronos/glTF -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/render/khronos/generator) find test/render/khronos/generator -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/render/outshine/grown) find test/render/outshine/grown -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/render/wpt/css) find test/render/wpt/css -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/render/test262/js) find test/render/test262/js -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    *) printf '%s' "" ;;
  esac
}

NotTheHarnesses() {
  case "$1" in
    harness/shared | harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown) printf '%s' "the harness's own clock and its prune, run by this script and judged by nobody" ;;
    harness/shared/render) printf '%s' "the render scoring instrument, compiled into each corpus's own harness" ;;
    tools/viewer/parts) printf '%s' "the browser's own declaration and its face, compiled into the browser" ;;
    tools/host) printf '%s' "a host's transports, which nothing compiles yet -- board:0069 is where they are spent" ;;
    unit/compile | unit/compile/*) printf '%s' "a compile subject, judged by the layer's own refusal test, never linked" ;;
    harness/render/khronos/glTF/prepare | harness/render/khronos/generator/prepare | harness/render/outshine/grown/prepare | harness/render/wpt/css/prepare | harness/render/test262/js/prepare) printf '%s' "how a corpus is obtained, run by test/harness/shared/corpus/prepare.py and never by this script" ;;
    harness/shared/corpus | harness/shared/corpus/*) printf '%s' "the offline preparer's own, compiled and run by test/harness/shared/corpus/prepare.py" ;;
    *) return 1 ;;
  esac
}

LayerExtraSources() {
  case "$1" in
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown) printf '%s' "test/harness/shared/render/Parity.cpp" ;;
    tools/viewer) printf '%s' "test/harness/shared/render/Parity.cpp tools/viewer/parts/Chrome.cpp" ;;
    render/outshine/world) printf '%s' "tools/host/CurlTransport.cpp" ;;
    tools/driver) printf '%s' "tools/host/CurlTransport.cpp" ;;
    tools/driver/stills | tools/driver/window) printf '%s' "tools/host/CurlTransport.cpp" ;;
    *) printf '%s' "" ;;
  esac
}

GroupIncludes() {
  case "$1" in
    src/core | src/core/io | src/core/Sha256.cpp | src/core/Json.cpp | src/core/Script.cpp) printf '%s' "-Isrc/core -Isrc/core/io" ;;
    src/actor/path) printf '%s' "-Isrc/actor/path" ;;
    src/scene) printf '%s' "-Iinclude/outshine" ;;
    src/actor/body) printf '%s' "-Isrc/actor/body" ;;
    src/actor/mind) printf '%s' "-Isrc/actor/path -Isrc/actor/mind" ;;
    src/sim/Rigging.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/actor/path -Isrc/actor/body -Isrc/actor/mind -Isrc/sim" ;;
    src/sim/DriveTick.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/actor/path -Isrc/data -Isrc/actor/body -Isrc/actor/mind -Isrc/scenario -Isrc/sim -Isrc/ground -Isrc/ground/tiles" ;;
    src/actor/path/Wayfinding.cpp) printf '%s' "-Isrc/actor/path" ;;
    src/ground/RoadHarvest.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/ground -Isrc/ground/tiles" ;;
    src/data) printf '%s' "-Isrc/core -Isrc/data" ;;
    src/ground | src/ground/tiles) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/ground -Isrc/ground/tiles" ;;
    src/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    src/ui) printf '%s' "-Isrc/core -Isrc/ui" ;;
    src/scenario | src/scenario/ScenarioRead.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/scenario" ;;
    src/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    src/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    src/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    src/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    src/render | src/render/stages | src/render/Readback.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages $(pkg-config --cflags sdl3)" ;;
    src/clients/GltfStudio.cpp | src/clients/Surfaces.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients $(pkg-config --cflags sdl3)" ;;
    src/clients/Live.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/ui $(pkg-config --cflags sdl3)" ;;
    src/clients/Assembly.cpp) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/clients" ;;
    src/sim) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/actor/body -Isrc/actor/mind -Isrc/scenario -Isrc/sim -Isrc/ground -Isrc/ground/tiles" ;;
    src/clients/Engine.cpp) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/scenario -Isrc/ui $(pkg-config --cflags sdl3)" ;;
    src/clients/Sim.cpp | src/clients/LogSinks.cpp | src/clients/StreamTelemetry.cpp | src/clients/EyeTelemetry.cpp | src/clients/CsvTelemetry.cpp | src/clients/Species.cpp | src/clients/RegionForge.cpp | src/clients/SceneWeather.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/data -Isrc/scenario -Isrc/ground -Isrc/ground/tiles -Isrc/generators -Isrc/generators/draw -Isrc/actor/path -Isrc/clients" ;;
    src/clients/Image.cpp) printf '%s' "-Isrc/clients $(pkg-config --cflags sdl3-image)" ;;
    *) return 1 ;;
  esac
}

GroupToolchain() {
  case "$1" in
    src/render | src/render/stages | src/render/Readback.cpp | src/clients/GltfStudio.cpp | src/clients/Surfaces.cpp | src/clients/Live.cpp) LayerToolchain render ;;
    *) printf '%s' "$CXXSTD" ;;
  esac
}

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
    setId=$(printf '%s|%s' "$groupIncludes" "$groupStd" | cksum | cut -d' ' -f1)
    unitObject=$OBJDIR/$(dirname "$unit" | tr / -)-$(basename "$unit" .cpp).$setId.o
    if ! UpToDate "$unitObject" "$unit"; then
      $CXX "$unit" $groupStd $OPT $WARN $SAN $EXTRA_DEFINES -MMD -MP $groupIncludes -c -o "$unitObject" || return 1
    fi
    OBJECTS="$OBJECTS $unitObject"
  done
  return 0
}

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
  wait "$child" 2>/dev/null
  status=$?
  KillRunning
  wait "$watchdog" 2>/dev/null
  return $status
}

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

# THE LIBRARY ENTIRE, from the same declarations the tests build with: every source under src/
# resolves to a group -- its own file arm when GroupIncludes names it, its directory otherwise --
# and a source neither names is a refusal, never a guess. The Makefile's `all` target is this call,
# so there is exactly ONE spelling of what compiles with what in the repository.
BuildLibrary() {
  OBJECTS=""
  libraryGroups=" "
  for libraryUnit in $(find src -name '*.cpp' | sort); do
    if GroupIncludes "$libraryUnit" >/dev/null 2>&1; then
      libraryGroup=$libraryUnit
    else
      libraryGroup=$(dirname "$libraryUnit")
    fi
    case "$libraryGroups" in *" $libraryGroup "*) continue ;; esac
    libraryGroups="$libraryGroups$libraryGroup "
    BuildGroup "$libraryGroup" || Die "the library group $libraryGroup did not build"
  done
  mkdir -p build
  rm -f build/liboutshine.a
  ar rcs build/liboutshine.a $OBJECTS || Die "the archive did not write"
  printf -- '-> build/liboutshine.a (%s objects)\n' "$(echo $OBJECTS | wc -w | tr -d ' ')"
}
if [ -n "${LIBRARY:-}" ]; then
  BuildLibrary
  trap - EXIT
  exit 0
fi

OBJECTS=""
BuildGroup src/core/Json.cpp || Die "the prune's reader did not build"
$CXX test/harness/shared/Prune.cpp $OBJECTS $CXXSTD $OPT $WARN -Itest/harness/shared -Itest/harness/shared/render -Isrc/core -o "$BUILD/prune" ||
  Die "the prune did not build"
PRUNE_MARKER=$BUILD/prune.marker

if [ "$AUDIT" = 1 ]; then
  bad=0
  for suiteDir in $(find test tools -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u); do
    groups=$(LayerGroups "$suiteDir" 2>/dev/null) || continue
    files=""
    for group in $groups; do
      if [ -d "$group" ]; then
        files="$files $(find "$group" -maxdepth 1 -name '*.cpp' | sort)"
      else
        files="$files $group"
      fi
    done
    dupes=$(printf '%s\n' $files | sort | uniq -d)
    if [ -n "$dupes" ]; then
      printf 'AUDIT %s lists twice: %s\n' "$suiteDir" "$dupes"
      bad=1
    fi
  done
  covered=$(for suiteDir in $(find test tools -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u); do
    groups=$(LayerGroups "$suiteDir" 2>/dev/null) || continue
    for group in $groups; do
      if [ -d "$group" ]; then find "$group" -maxdepth 1 -name '*.cpp'; else printf '%s\n' "$group"; fi
    done
  done | sort -u)
  for source in $(find src -name '*.cpp' | sort); do
    printf '%s\n' "$covered" | grep -qx "$source" || { printf 'AUDIT no suite compiles %s\n' "$source"; bad=1; }
  done
  [ "$bad" = 0 ] && printf 'AUDIT clean: every suite lists each source once, every source has a suite\n'
  exit $bad
fi

# every DECLARED suite's object set is closed over its own outshine symbols (board:1641):
# a sources list that lost a unit refuses here by name, not at a sporadic link
if [ "$AUDITLINK" = 1 ]; then
  bad=0
  auditSuites=$SUITES
  [ -n "$auditSuites" ] ||
    auditSuites=$(find test tools -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u)
  for suiteDir in $auditSuites; do
    groups=$(LayerGroups "$suiteDir" 2>/dev/null) || continue
    [ -n "$groups" ] || continue
    OBJECTS=""
    built=yes
    for group in $groups; do
      BuildGroup "$group" || { built=no; break; }
    done
    if [ "$built" = no ]; then
      printf 'AUDIT %s does not compile under its own declaration\n' "$suiteDir"
      bad=1
      continue
    fi
    nm -u $OBJECTS 2>/dev/null | grep -o '__ZN8outshine[A-Za-z0-9_]*' | sort -u > "$BUILD/audit.undef"
    nm -gU $OBJECTS 2>/dev/null | awk '{print $3}' | grep '^__ZN8outshine' | sort -u > "$BUILD/audit.def"
    unresolved=$(comm -23 "$BUILD/audit.undef" "$BUILD/audit.def")
    if [ -n "$unresolved" ]; then
      printf 'AUDIT %s cannot resolve: %s\n' "$suiteDir" "$(printf '%s' "$unresolved" | head -3 | tr '\n' ' ')"
      bad=1
    fi
  done
  [ "$bad" = 0 ] && printf 'AUDIT closed: every declared suite resolves its own symbols from its own objects\n'
  exit $bad
fi

TREES=test
for named in $SUITES; do
  case "$named" in tools | tools/*) TREES="test tools" ;; esac
done

TESTS=""
for candidate in $(find $TREES -name '*.cpp' | sort); do
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

NAMED=0
for named in $SUITES; do NAMED=$((NAMED + 1)); done

CASE=
if [ "$NAMED" -gt 1 ] && [ -f "test/$SUITE/manifest.json" ]; then
  Die "a single case is named on its own -- $SUITE carries a manifest and $NAMED suites were named"
fi
if [ "$NAMED" -eq 1 ] && [ -f "test/$SUITE/manifest.json" ]; then
  for candidate in $(for one in $TESTS; do dirname "${one#test/}"; done | sort -u); do
    if LayerCases "$candidate" | grep -qxF "test/$SUITE"; then
      CASE=test/$SUITE
      SUITE=$candidate
      break
    fi
  done
  [ -n "$CASE" ] ||
    Die "test/$SUITE carries a manifest and no declared layer enumerates it -- add it to LayerCases, or name the suite instead"
  SUITES=" $SUITE"
fi

# THE FAST GATE IS THE DEFAULT (board:1601): run.sh without suites runs the regression gate --
# the unit mirror, the claims, and the door proof -- and EXCLUDES the named-only suites, loudly.
# The long suites (device corpora, oracle renders, the drive) run only when named: sporadic by
# rule, never per edit. kFastGateBoundMs is [SET] 90000 ms on this machine over the WARM
# population (measured 48.6 s tests + 1.4 s library, 119 tests, 2026-08-22); a COLD run after a
# flag change rebuilds every set-stamped object and overruns once by design -- its red says
# 'run again warm', which the overrun message states.
NAMED_ONLY="harness/render render/outshine/drive render/outshine/frame render/outshine/scenario render/outshine/shader render/outshine/world tools"
FAST_GATE=no
kFastGateBoundMs=90000
if [ -z "$SUITES" ]; then
  FAST_GATE=yes
  kept=""
  for candidate in $TESTS; do
    rel=${candidate#test/}
    fast=yes
    for slow in $NAMED_ONLY; do
      case "$rel" in "$slow"/* | "$slow") fast=no ;; esac
    done
    [ "$fast" = yes ] && kept="$kept $candidate"
  done
  TESTS=$kept
  printf 'run.sh: the fast gate -- named-only suites excluded: %s\n' "$NAMED_ONLY"
  gateLibraryFrom=$(Now)
  BuildLibrary
  printf 'run.sh: the gate compiled the library entire in %s ms\n' "$(( $(Now) - gateLibraryFrom ))"
fi

if [ -n "$SUITES" ]; then
  selected=""
  for named in $SUITES; do
    under=""
    for candidate in $TESTS; do
      case "${candidate#test/}" in "$named"/*) under="$under $candidate" ;; esac
    done
    [ -n "$under" ] ||
      Die "no declared suite under $named -- $(find $TREES -name '*.cpp' -exec dirname {} \; | sed 's|^test/||' | sort -u | tr '\n' ' ')"
    selected="$selected$under"
  done
  TESTS=$(printf '%s\n' $selected | sort -u | tr '\n' ' ')
  if [ -n "$CASE" ]; then
    printf 'run.sh: %s only, under %s\n' "$CASE" "$SUITE"
  else
    printf 'run.sh:%s only\n' "$SUITES"
  fi
fi

started=$(Now)
passed=0
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
uiReduced=0
uiOutside=0
jsInside=0
jsReduced=0
jsOutside=0
jsHeld=0
jsRed=0
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

PreparedCase() { printf '%s/%s' "$PREPARED" "$(printf '%s' "$1" | tr / -)"; }

SuiteKib() {
  set -- $(du -sk "$PREPARED" 2>/dev/null)
  printf '%s' "${1:-0}"
}

SampleSuite() {
  sampled=$(SuiteKib)
  [ "$sampled" -gt "$peakKib" ] && peakKib=$sampled
  return 0
}

PruneCase() {
  pruneCase=$1
  prunePrepared=$2
  pruneLog=$BUILD/log/$(printf '%s' "${pruneCase#test/}" | tr / -)-prune.log
  if ! pruneSummary=$("$BUILD/prune" "$prunePrepared" "$PRUNE_MARKER" 2>"$pruneLog"); then
    printf 'run.sh: %s was not pruned -- %s\n' "${pruneCase#test/}" "$pruneLog" >&2
    return 0
  fi
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

CountTheTwo() {
  case "$1" in *'~'*) return 0 ;; esac
  [ -f "$2" ] || return 0
  subset=$(sed -n 's/^UI-SUBSET //p' "$2" | head -1)
  layout=$(sed -n 's/^UI-LAYOUT //p' "$2" | head -1)
  case "$subset" in
    inside) uiInside=$((uiInside + 1)) ;;
    reduced) uiReduced=$((uiReduced + 1)) ;;
    outside) uiOutside=$((uiOutside + 1)) ;;
  esac
  case "$layout" in
    held) uiHeld=$((uiHeld + 1)) ;;
    red) uiRed=$((uiRed + 1)) ;;
  esac
  jsSubset=$(sed -n 's/^JS-SUBSET //p' "$2" | head -1)
  jsCase=$(sed -n 's/^JS-CASE //p' "$2" | head -1)
  case "$jsSubset" in
    inside) jsInside=$((jsInside + 1)) ;;
    reduced) jsReduced=$((jsReduced + 1)) ;;
    outside) jsOutside=$((jsOutside + 1)) ;;
  esac
  case "$jsCase" in
    held) jsHeld=$((jsHeld + 1)) ;;
    red) jsRed=$((jsRed + 1)) ;;
  esac
  criterion=$(sed -n 's/^KHRONOS-CRITERION //p' "$2" | head -1)
  picture=$(sed -n 's/^PICTURE-BOUND //p' "$2" | head -1)
  case "$1" in
    render/khronos/*)
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
  for freshNeed in $(tr '\\' ' ' <"$freshBinary.d" | tr ':' ' ' | tr -s ' \n' ' '); do
    case "$freshNeed" in *.o|*.cpp|*.h|*.hpp) ;; *) continue ;; esac
    [ -f "$freshNeed" ] || return 1
    [ "$freshNeed" -nt "$freshBinary" ] && return 1
  done
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

JudgeInstrument() {
  [ "$2" = yes ] || return 0
  grep -qE "AddressSanitizer|runtime error:" "$log" || return 0
  printf 'run.sh: %s -- the run finished and the sanitiser spoke, %s\n' "$1" "$log" >&2
  failed=$((failed + 1))
}

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
  compileDefine="-DOUTSHINE_COMPILE=\"$CXX $CXXSTD $WARN $includes\""
  if [ "$built" = yes ]; then
    buildCommand="$CXX $testSource $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $includes $compileDefine $linkage"
    if Fresh "$plainBinary" "$buildCommand" $testSource $(LayerExtraSources "$layer"); then :; else
      $CXX "$testSource" $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN -Itest/harness/shared -Itest/harness/shared/render $includes "$compileDefine" $linkage -MMD -MP -MF "$plainBinary.d" -o "$plainBinary" >>"$log" 2>&1 && printf '%s' "$buildCommand" >"$plainBinary.cmd" || built=no
    fi
  fi

  if [ "$built" = no ]; then
    failures=0
    skips=0
    verdict=BUILD
    Record "$id" "$(( $(Now) - before ))"
    continue
  fi

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

  cases=$(LayerCases "$layer")
  if [ -z "$cases" ]; then
    JudgeArms "$id" ""
    continue
  fi

  oldIfs=$IFS
  IFS='
'
  for oneCase in $cases; do
    IFS=$oldIfs
    if [ -n "$CASE" ] && [ "$oneCase" != "$CASE" ]; then IFS='
'; continue; fi
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
elapsedMs=$(( $(Now) - started ))
printf '%s tests: %s PASS  %s FAIL  %s TIMEOUT  %s SIGNAL  %s BUILD  %s SKIP  %s UNPREPARED  in %s ms\n' \
  "$total" "$passed" "$failed" "$timedout" "$signalled" "$unbuilt" "$skipped" "$unprepared" \
  "$elapsedMs"
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
[ $((uiInside + uiReduced + uiOutside)) -gt 0 ] &&
  printf 'wpt:     %s held, %s reduced, %s unaccounted of %s   (%s attempted, %s red)\n' \
    "$uiHeld" "$uiReduced" "$uiOutside" "$((uiInside + uiReduced + uiOutside))" \
    "$uiInside" "$uiRed"
[ $((jsInside + jsReduced + jsOutside)) -gt 0 ] &&
  printf 'test262: %s held, %s reduced, %s unaccounted of %s   (%s attempted, %s red)\n' \
    "$jsHeld" "$jsReduced" "$jsOutside" "$((jsInside + jsReduced + jsOutside))" \
    "$jsInside" "$jsRed"
[ "$validatedRan" = yes ] && printf '%s\n' \
  "~validated is an API-CONTRACT arm: it says the pipelines, passes and resources agree with the driver, and NOTHING about whether the picture is right -- that is render/'s domain and its oracle's"
[ "$inverted" -gt 0 ] && printf 'expect-fail inverted: %s\n' "$EXPECT_FAIL"

[ "$prunedCases" -gt 0 ] && printf \
  'test corpora: peak %s MB, %s MB after the last prune -- %s cases pruned, %s files and %s MB declined, %s file(s) left standing (each case: %s/*-prune.log)\n' \
  "$((peakKib / 1024))" "$((endKib / 1024))" "$prunedCases" "$prunedFiles" "$((prunedKib / 1024))" \
  "$stayedFiles" "$BUILD/log"

if [ "$FAST_GATE" = yes ] && [ "$elapsedMs" -gt "$kFastGateBoundMs" ]; then
  printf 'run.sh: THE FAST GATE OVERRAN ITS BOUND -- %s ms over the declared %s ms (warm population): a slow test is a finding, exactly like a slow frame. A cold run after a flag change rebuilds every set-stamped object -- run again warm before judging (board:1601)\n' \
    "$elapsedMs" "$kFastGateBoundMs" >&2
  exit 1
fi

red=$((failed + timedout + signalled + unbuilt + undeclaredSkips + unprepared))
[ "$red" -eq 0 ] || exit 1
exit 0
