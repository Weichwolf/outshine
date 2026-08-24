#!/bin/sh

set -u
set -m
AUDIT=0
CORPUS=0
WOULDPRUNE=0
CASELIST=0
AUDITLINK=0

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 2

BUILD=${TMPDIR:-/tmp}
# the nest carries the checkout identity (board:1649): parallel checkouts get parallel
# nests, a worktree gate cannot sweep this one mid-run, and a collision is unspellable
NEST=$(printf %s "$ROOT" | shasum -a 256 | cut -c1-12)
BUILD=${BUILD%/}/outshine-tests.$NEST
INHERITED_NEST=${OUTSHINE_NEST:-}
export OUTSHINE_NEST="$BUILD"
PREPARED=${TMPDIR:-/tmp}
PREPARED=${PREPARED%/}/outshine-prepared
PREPARED=${OUTSHINE_PREPARED:-$PREPARED}
CXX=${CXX:-c++}
CXXSTD=-std=c++23
WARN="-Wall -Wextra -Wpedantic -Wshadow -Werror -Wno-unused-parameter"
OPT=-O2

TIMEOUT_S=120
ALLOWED_SKIPS=""
EXTRA_DEFINES=""
validatedRan=no

# a case listed here is a case whose RED is a standing finding, not a licence. Behaving as
# declared -- failing exactly the stated number of claims -- turns the verdict PASS and counts
# toward `inverted`, which the trailer names. Behaving OTHERWISE, including going fully green,
# prints to stderr and turns the verdict FAIL, which is what forces the entry out again
EXPECT_FAIL="harness/claims/ExpectFail:1 unit/actor/path/ACurveIsFittedAtTheRadiusItHas:1"

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

ReleaseNest() {
  if [ "${NESTLOCK_MINE:-no}" = yes ]; then
    rm -f "$NESTLOCK"
    NESTLOCK_MINE=no
  fi
}
trap 'KillRunning; ReleaseNest; ReleaseCorpus; exit 130' INT
trap 'KillRunning; ReleaseNest; ReleaseCorpus; exit 143' TERM
trap 'KillRunning; ReleaseNest; ReleaseCorpus; exit 129' HUP
CORPUSLOCK=$PREPARED.lock
CORPUSLOCK_MINE=no
ClaimCorpus() {
  [ -d "$PREPARED" ] || return 0
  if (set -C; printf '%s' "$$" > "$CORPUSLOCK") 2>/dev/null; then
    CORPUSLOCK_MINE=yes
    return 0
  fi
  corpusHolder=$(cat "$CORPUSLOCK" 2>/dev/null)
  if [ -n "$corpusHolder" ] && kill -0 "$corpusHolder" 2>/dev/null; then
    printf 'run.sh: another runner (pid %s) is reading the shared corpus, so this run will NOT prune it -- the corpus is shared between checkouts and pruning it is a delete (board:1789)\n' \
      "$corpusHolder" >&2
    return 0
  fi
  rm -f "$CORPUSLOCK" 2>/dev/null
  if (set -C; printf '%s' "$$" > "$CORPUSLOCK") 2>/dev/null; then CORPUSLOCK_MINE=yes; fi
  return 0
}
ReleaseCorpus() {
  [ "$CORPUSLOCK_MINE" = yes ] && rm -f "$CORPUSLOCK" 2>/dev/null
  CORPUSLOCK_MINE=no
  return 0
}

trap 'KillRunning; ReleaseNest; ReleaseCorpus' EXIT

ClaimCorpus

NESTLOCK=$BUILD.lock
NESTLOCK_MINE=no
if [ "$INHERITED_NEST" != "$BUILD" ]; then
  # the claim and the identity are ONE atomic step: noclobber-create the lock FILE with the
  # pid already inside -- no window in which a second runner reads an empty claim. A stale
  # lock (dead pid) is removed and the claim retried under the same noclobber, so of two
  # stale-breakers exactly one wins and the other refuses.
  claimAttempts=0
  while :; do
    if (set -C; printf '%s' "$$" > "$NESTLOCK") 2>/dev/null; then
      NESTLOCK_MINE=yes
      break
    fi
    otherPid=$(cat "$NESTLOCK" 2>/dev/null)
    if [ -n "$otherPid" ] && kill -0 "$otherPid" 2>/dev/null; then
      Die "another runner (pid $otherPid) holds this checkout's nest -- two gates in one nest read each other's half-written objects, so the second refuses instead of corrupting both"
    fi
    claimAttempts=$((claimAttempts + 1))
    [ "$claimAttempts" -le 6 ] || Die "the nest lock at $NESTLOCK would not settle after six breaks -- inspect it by hand"
    if [ -z "$otherPid" ] && [ "$claimAttempts" -le 3 ]; then
      # a claim exists but names nobody yet: creation and pid are one write, so this is a
      # writer mid-flight or a corpse -- give it a beat before treating it as dead
      sleep 1
      continue
    fi
    # the claim looks dead: STEAL it atomically and inspect what was caught -- a claim that
    # turned live mid-break is put straight back and refused, never corrupted
    if mv "$NESTLOCK" "$NESTLOCK.stolen.$$" 2>/dev/null; then
      stolenPid=$(cat "$NESTLOCK.stolen.$$" 2>/dev/null)
      if [ -n "$stolenPid" ] && kill -0 "$stolenPid" 2>/dev/null; then
        ln "$NESTLOCK.stolen.$$" "$NESTLOCK" 2>/dev/null || :
        rm -f "$NESTLOCK.stolen.$$"
        Die "the nest's claim turned live mid-break (pid $stolenPid) -- refusing rather than corrupting two gates"
      fi
      rm -f "$NESTLOCK.stolen.$$"
    fi
  done
fi

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
    --corpus) CORPUS=1; shift ;;
    --would-prune) WOULDPRUNE=1; shift ;;
    --cases) CASELIST=1; shift ;;
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
    unit/data) printf '%s' "-Iinclude -Isrc/core -Isrc/data" ;;
    unit/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    unit/ui) printf '%s' "-Isrc/core -Isrc/ui" ;;
    unit/scenario) printf '%s' "-Iinclude -Isrc/core -Isrc/scenario" ;;
    unit/audio) printf '%s' "-Iinclude -Isrc/core -Isrc/audio -Isrc/scenario" ;;
    unit/generators) printf '%s' "-Isrc/core -Isrc/generators" ;;
    unit/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    unit/ground) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/ground -Isrc/ground/tiles" ;;
    unit/ground/tiles) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/data -Isrc/ground -Isrc/ground/tiles" ;;
    unit/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    unit/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    unit/render/stages) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages $(pkg-config --cflags sdl3)" ;;
    unit/render) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages" ;;
    unit/clients) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/actor/path -Isrc/actor/body -Isrc/actor/mind -Isrc/scenario -Isrc/generators -Isrc/ground -Isrc/data -Isrc/clients" ;;
    harness/claims) printf '%s' "-Isrc/core" ;;
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/data -Isrc/scene -Isrc/scenario -Isrc/ui -Iinclude -Isrc/clients" ;;
    harness/render/wpt/css) printf '%s' "-Isrc/core -Isrc/ui" ;;
    harness/render/test262/js) printf '%s' "-Isrc/core" ;;
    render/outshine/frame) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/ui" ;;
    render/outshine/shader) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render -Isrc/render/draw -Isrc/render/plan -Isrc/render/stages" ;;
    render/outshine/client) printf '%s' "-Iinclude" ;;
    render/outshine/world) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/data -Isrc/scenario -Isrc/generators -Isrc/generators/draw -Isrc/ground -Isrc/ground/tiles -Isrc/clients -Isrc/actor/path -Itools/host" ;;
    render/outshine/drive) printf '%s' "-Iinclude -Isrc/actor/path -Isrc/actor/body -Isrc/actor/mind" ;;
    render/outshine/scenario) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/scenario -Isrc/ui -Itest/harness/shared" ;;
    tools/viewer) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/data -Isrc/clients -Isrc/ui -Iinclude -Itools/viewer/parts" ;;
    apps/driver/test/stills | apps/driver/test/window) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/clients -Isrc/actor/path -Isrc/data -Isrc/gltf -Isrc/actor/body -Isrc/actor/mind -Isrc/render -Isrc/render/plan -Isrc/render/draw -Isrc/render/stages -Isrc/scenario -Isrc/ui -Isrc/ground -Isrc/ground/tiles -Itools/host -Isrc/sim" ;;
    apps/driver/test) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/clients -Isrc/actor/path -Isrc/data -Isrc/actor/body -Isrc/actor/mind -Isrc/gltf -Isrc/scenario -Isrc/ground -Isrc/ground/tiles -Itools/host -Isrc/sim" ;;
    *) return 1 ;;
  esac
}

LayerToolchain() {
  case "$1" in
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/frame | tools/viewer | render/outshine/scenario) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    render/outshine/client) printf '%s' "$CXXSTD" ;;
    render/outshine/drive) printf '%s' "$CXXSTD" ;;
    apps/driver/test) printf '%s' "$CXXSTD" ;;
    apps/driver/test/stills | apps/driver/test/window) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    render/outshine/shader) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3)" ;;
    unit/clients) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3-image)" ;;
    unit/render | unit/render/stages) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3)" ;;
    *) printf '%s' "$CXXSTD" ;;
  esac
}
# ONE case is exempt, by name and with its reason: the heap ledger measures the tree's own
# operator new, and ASan REPLACES that allocator -- sanitising it would measure ASan, not
# the instrument. The exemption is a case, never a layer (board:1743).
SANITISER_EXEMPT="unit/core/EveryByteTheHeapTakesLandsUnderATagOrUnderOther"

LayerSanitiser() {
  case "$1" in
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/shader | unit/ui | unit/core | unit/gltf | unit/data | unit/ground | unit/ground/tiles) printf '%s' "-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1" ;;
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
    apps/driver/test) printf '%s' "-lz -lcurl" ;;
    apps/driver/test/stills | apps/driver/test/window) printf '%s' "$(pkg-config --libs sdl3) $(pkg-config --libs sdl3-image) -lz -lcurl" ;;
    render/outshine/shader) printf '%s' "$(pkg-config --libs sdl3) -lz" ;;
    unit/clients) printf '%s' "$(pkg-config --libs sdl3-image) -lz" ;;
    unit/render | unit/render/stages) printf '%s' "$(pkg-config --libs sdl3) -lz" ;;
    unit/core | unit/ground | unit/ground/tiles | unit/data | unit/render/plan | unit/render/draw | unit/sim | unit/actor/path) printf '%s' "-lz" ;;
    *) printf '%s' "" ;;
  esac
}

LayerGroups() {
  case "$1" in
    unit/core) printf '%s' "src/core src/core/io" ;;
    unit/core/io) printf '%s' "src/core src/core/io" ;;
    unit/actor/path) printf '%s' "src/actor/path" ;;
    unit/scene) printf '%s' "src/scene" ;;
    unit/sim) printf '%s' "src/core src/core/io src/actor/path src/data src/actor/body src/actor/mind src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/sim src/scene src/ground src/ground/tiles" ;;
    unit/actor/body) printf '%s' "src/actor/body" ;;
    unit/actor/mind) printf '%s' "src/actor/path src/actor/mind" ;;
    unit/data) printf '%s' "src/core src/core/io src/data" ;;
    unit/gltf) printf '%s' "src/core src/gltf" ;;
    unit/ui) printf '%s' "src/core src/ui" ;;
    harness/render/wpt/css) printf '%s' "src/core/Json.cpp src/ui" ;;
    harness/render/test262/js) printf '%s' "src/core/Json.cpp src/core/Script.cpp" ;;
    unit/scenario) printf '%s' "src/core src/scenario" ;;
    unit/audio) printf '%s' "src/core src/audio src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp" ;;
    unit/generators) printf '%s' "src/core src/generators" ;;
    unit/generators/draw) printf '%s' "src/core src/generators src/generators/draw" ;;
    unit/ground) printf '%s' "src/core src/core/io src/actor/path src/data src/ground src/ground/tiles" ;;
    unit/ground/tiles) printf '%s' "src/core src/core/io src/ground/tiles" ;;
    unit/render/plan) printf '%s' "src/core src/core/io src/render/plan" ;;
    unit/render/draw) printf '%s' "src/core src/core/io src/render/draw" ;;
    unit/render/stages) printf '%s' "src/core src/core/io src/render/plan src/render/draw src/render src/render/stages" ;;
    unit/render) printf '%s' "src/core src/core/io src/render/plan src/render/draw src/render src/render/stages" ;;
    unit/clients) printf '%s' "src/core src/core/io src/actor/path src/actor/body src/actor/mind src/scene src/generators src/clients/Image.cpp src/clients/Species.cpp src/clients/RegionForge.cpp src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/scenario/InputMap.cpp src/clients/Assembly.cpp src/clients/InputPump.cpp" ;;
    harness/claims) printf '%s' "src/core/Sha256.cpp src/core/Json.cpp" ;;
    harness/render/khronos/glTF | harness/render/khronos/generator | harness/render/outshine/grown | render/outshine/frame | tools/viewer | render/outshine/scenario | render/outshine/client) printf '%s' "src/core src/core/io src/gltf src/render/plan src/render/draw src/render src/render/stages src/scene src/ui src/clients/GltfStudio.cpp src/clients/Image.cpp src/clients/Surfaces.cpp src/clients/Live.cpp src/clients/Engine.cpp src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/clients/Assembly.cpp" ;;
    render/outshine/shader) printf '%s' "src/core src/core/io src/render/plan src/render/draw src/render src/render/stages" ;;
    apps/driver/test/stills | apps/driver/test/window) printf '%s' "src/core src/core/io src/gltf src/render/plan src/render/draw src/render src/render/stages src/ui src/actor/path src/actor/body src/actor/mind src/data src/ground/tiles src/ground src/clients/GltfStudio.cpp src/clients/Image.cpp src/clients/Surfaces.cpp src/clients/Live.cpp src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/sim src/scene src/clients/Assembly.cpp" ;;
    apps/driver/test) printf '%s' "src/core src/core/io src/gltf src/actor/path src/actor/body src/actor/mind src/data src/ground/tiles src/ground src/sim src/scene src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/clients/Assembly.cpp" ;;
    render/outshine/world) printf '%s' "src/core src/core/io src/data src/scenario src/generators src/generators/draw src/ground/tiles src/ground src/actor/path/Wayfinding.cpp src/clients/Sim.cpp src/clients/LogSinks.cpp src/clients/StreamTelemetry.cpp src/clients/EyeTelemetry.cpp src/clients/Species.cpp src/clients/RegionForge.cpp" ;;
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
    apps/driver/test) printf '%s' "tools/host/CurlTransport.cpp" ;;
    apps/driver/test/stills | apps/driver/test/window) printf '%s' "tools/host/CurlTransport.cpp" ;;
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
    src/ground/RoadHarvest.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/ground -Isrc/ground/tiles" ;;
    src/data) printf '%s' "-Iinclude -Isrc/core -Isrc/data" ;;
    src/ground | src/ground/tiles) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/ground -Isrc/ground/tiles" ;;
    src/gltf) printf '%s' "-Isrc/core -Isrc/gltf" ;;
    src/ui) printf '%s' "-Isrc/core -Isrc/ui" ;;
    src/audio) printf '%s' "-Iinclude -Isrc/core -Isrc/audio" ;;
    src/scenario | src/scenario/ScenarioRead.cpp | src/scenario/ScenarioLayer.cpp | src/scenario/InputMap.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/scenario" ;;
    src/generators | src/generators/TreeSpecies.cpp | src/generators/GrowthForm.cpp) printf '%s' "-Isrc/core -Isrc/generators" ;;
    src/generators/draw) printf '%s' "-Isrc/core -Isrc/generators -Isrc/generators/draw" ;;
    src/render/plan) printf '%s' "-Isrc/core -Isrc/render/plan" ;;
    src/render/draw) printf '%s' "-Isrc/core -Isrc/render/draw" ;;
    src/render | src/render/stages | src/render/Readback.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages $(pkg-config --cflags sdl3)" ;;
    src/clients/GltfStudio.cpp | src/clients/Surfaces.cpp) printf '%s' "-Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients $(pkg-config --cflags sdl3)" ;;
    src/clients/Live.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/ui $(pkg-config --cflags sdl3)" ;;
    src/clients/Assembly.cpp) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/clients" ;;
    src/clients/InputPump.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/scenario -Isrc/clients $(pkg-config --cflags sdl3)" ;;
    src/sim) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/actor/path -Isrc/data -Isrc/actor/body -Isrc/actor/mind -Isrc/scenario -Isrc/sim -Isrc/ground -Isrc/ground/tiles" ;;
    src/clients/Engine.cpp) printf '%s' "-Iinclude -Iinclude/outshine -Isrc/core -Isrc/core/io -Isrc/data -Isrc/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages -Isrc/clients -Isrc/scenario -Isrc/ui $(pkg-config --cflags sdl3)" ;;
    src/clients/Sim.cpp | src/clients/LogSinks.cpp | src/clients/StreamTelemetry.cpp | src/clients/EyeTelemetry.cpp | src/clients/Species.cpp | src/clients/RegionForge.cpp) printf '%s' "-Iinclude -Isrc/core -Isrc/core/io -Isrc/data -Isrc/scenario -Isrc/ground -Isrc/ground/tiles -Isrc/generators -Isrc/generators/draw -Isrc/actor/path -Isrc/clients" ;;
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

# freshness is CONTENT, not clock: a checkout, a stash pop or a revert moves an mtime
# BACKWARDS, and "-nt" then reports a stale object as current -- a green verdict about
# source nobody compiled (board:1751). The stamp beside each object is the digest of the
# source and every prerequisite the compiler named.
# the stamp is (mtime, size, name) of the source and every prerequisite, gathered in ONE
# stat call: a backwards mtime DIFFERS from the recorded one, where "-nt" called it current
SourceStamp() {
  stampSource=$1
  stampDeps=$2
  stampFiles=$stampSource
  if [ -f "$stampDeps" ]; then
    stampFiles="$stampFiles $(sed -e 's/^[^:]*://' -e 's/\\//g' "$stampDeps")"
  fi
  stat -f '%m|%z|%N' -- $stampFiles 2>/dev/null || return 1
}

UpToDate() {
  objectPath=$1
  sourcePath=$2
  depsPath=${objectPath%.o}.d
  stampPath=${objectPath%.o}.stamp
  [ -f "$objectPath" ] || return 1
  [ -f "$depsPath" ] || return 1
  [ -f "$stampPath" ] || return 1
  freshStamp=$(SourceStamp "$sourcePath" "$depsPath") || return 1
  [ "$freshStamp" = "$(cat "$stampPath")" ] || return 1
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
    # the id carries the WHOLE compile line: includes, std, optimisation and warnings --
    # editing -O2 or the warning set in this file silently reused objects built with the
    # old one, and two of five flag groups is not an identity (board:1751)
    setId=$(printf '%s|%s|%s|%s' "$groupIncludes" "$groupStd" "$OPT" "$WARN" | cksum | cut -d' ' -f1)
    unitObject=$OBJDIR/$(dirname "$unit" | tr / -)-$(basename "$unit" .cpp).$setId.o
    if ! UpToDate "$unitObject" "$unit"; then
      $CXX "$unit" $groupStd $OPT $WARN $SAN $EXTRA_DEFINES -MMD -MP $groupIncludes -c -o "$unitObject" || return 1
      SourceStamp "$unit" "${unitObject%.o}.d" >"${unitObject%.o}.stamp"
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
  # board:1778: a case that is killed by the budget measured NOTHING. Handing it the budget lets
  # it stop inside one and REPORT, which is a measurement, instead of being cut off mid-drive.
  export OUTSHINE_TIMEOUT_S="$TIMEOUT_S"
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
TRAILER_PARTIAL=0
FAST_GATE=no
compileBlind=0
KILLED_BY_TIME=""
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
  [ $# -eq 10 ] && [ "$1" = CHECKS ] && [ "$3" = FAILURES ] && [ "$5" = SKIPPED ] &&
    [ "$7" = UNPREPARED ] && [ "$9" = PARTIAL ] ||
    Die "$trailerId printed a verdict line the reporter cannot have written -- $trailerLog"
  Number "$2" && Number "$4" && Number "$6" && Number "$8" && Number "${10}" ||
    Die "$trailerId printed a verdict line whose counts are not numbers -- $trailerLog"
  TRAILER_CHECKS=$2
  TRAILER_FAILURES=$4
  TRAILER_SKIPS=$6
  TRAILER_UNPREPARED=$8
  TRAILER_PARTIAL=${10}
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

EverySourceStillCompiles() {
  compiled=0
  broken=0
  seenLayers=""
  for candidate in $TESTS_ALL; do
    rel=${candidate#test/}
    layer=${rel%/*}
    ran=no
    for kept in $TESTS; do [ "$kept" = "$candidate" ] && ran=yes; done
    [ "$ran" = yes ] && continue
    includes="-Itest/harness/shared -Itest/harness/shared/render $(LayerIncludes "$layer")"
    toolchain=$(LayerToolchain "$layer")
    beside=""
    case " $seenLayers " in
      *" $layer "*) ;;
      *)
        seenLayers="$seenLayers $layer"
        for extra in $(LayerExtraSources "$layer"); do
          case "$extra" in *.cpp) beside="$beside $extra" ;; esac
        done
        ;;
    esac
    for one in "$candidate" $beside; do
      if said=$($CXX -fsyntax-only $CXXSTD $toolchain $WARN $includes \
                  -DOUTSHINE_COMPILE="\"$CXX $CXXSTD\"" "$one" 2>&1); then
        compiled=$((compiled + 1))
      else
        broken=$((broken + 1))
        printf 'run.sh: %s does not COMPILE under %s, and the gate stands aside from RUNNING that suite -- a translation unit nobody compiles is a warning set nobody enforces (board:1766)\n' \
          "$one" "$layer" >&2
        printf '%s\n' "$said" | head -4 >&2
      fi
    done
  done
  printf 'run.sh: %s source(s) the gate did not run still compile, %s do not\n' \
    "$compiled" "$broken"
  [ "$broken" -eq 0 ]
}

WhatNoCorpusJudges() {
  for family in test/render/*/; do
    family=${family%/}
    [ "$family" = test/render/outshine ] && continue
    declared=$(find "$family" -mindepth 2 -maxdepth 3 -type d 2>/dev/null | wc -l | tr -d ' ')
    [ "${declared:-0}" -eq 0 ] && continue
    stem=$(printf '%s' "$family" | tr / -)
    fetched=$(find "$PREPARED" -maxdepth 2 -type f -path "$PREPARED/$stem-*" \
      ! -name manifest.json ! -name provenance.json 2>/dev/null | head -1)
    [ -z "$fetched" ] &&
      printf 'run.sh: %s declares %s cases and NOT ONE holds a fetched subject -- this trailer says nothing about it, and naming it would not either until test/harness/shared/corpus/prepare.py has run\n' \
        "$family" "$declared"
  done
  return 0
}

if [ "$CORPUS" = 1 ]; then
  WhatNoCorpusJudges
  exit 0
fi

if [ "$WOULDPRUNE" = 1 ]; then
  if [ "$CORPUSLOCK_MINE" = yes ]; then
    printf 'run.sh: this runner holds the corpus claim and WOULD prune\n'
  else
    printf 'run.sh: this runner does not hold the corpus claim and would NOT prune\n'
  fi
  exit 0
fi

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
# a sources list that lost a unit refuses here by name, not at a sporadic link. The audit
# reads the EXACT set-stamped, up-to-date object each declaration names; a missing or stale
# one is built once per group, a declared source that does not exist refuses as a ghost
if [ "$AUDITLINK" = 1 ]; then
  bad=0
  auditSuites=$SUITES
  [ -n "$auditSuites" ] ||
    auditSuites=$(find test tools -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u)
  for suiteDir in $auditSuites; do
    groups=$(LayerGroups "$suiteDir" 2>/dev/null) || continue
    [ -n "$groups" ] || continue
    OBJECTS=""
    stragglers=ok
    for group in $groups; do
      case "$group" in
        *.cpp)
          if [ ! -e "$group" ]; then
            printf 'AUDIT %s declares %s and no such source exists -- a ghost in the listing\n' "$suiteDir" "$group"
            stragglers=refused
            break
          fi
          ;;
      esac
      groupIncludes=$(GroupIncludes "$group") || {
        printf 'AUDIT %s declares %s and no include set is declared for it\n' "$suiteDir" "$group"
        stragglers=refused
        break
      }
      groupStd=$(GroupToolchain "$group")
      # the id carries the WHOLE compile line: includes, std, optimisation and warnings --
    # editing -O2 or the warning set in this file silently reused objects built with the
    # old one, and two of five flag groups is not an identity (board:1751)
    setId=$(printf '%s|%s|%s|%s' "$groupIncludes" "$groupStd" "$OPT" "$WARN" | cksum | cut -d' ' -f1)
      case "$group" in
        *.cpp) groupUnits=$group ;;
        *) groupUnits=$(find "$group" -maxdepth 1 -name '*.cpp' | sort) ;;
      esac
      pairs=$(printf '%s\n' $groupUnits | awk -v id="$setId" \
        '{ u=$0; s=u; sub(/\.cpp$/, "", s); gsub(/\//, "-", s); print u " " s "." id ".o" }')
      groupBuilt=no
      set -- $pairs
      while [ $# -ge 2 ]; do
        unit=$1
        objName=$2
        shift 2
        if [ ! -e "$unit" ]; then
          printf 'AUDIT %s declares %s and no such source exists -- a ghost in the listing\n' "$suiteDir" "$unit"
          stragglers=refused
          break
        fi
        if UpToDate "$OBJDIR/$objName" "$unit"; then
          OBJECTS="$OBJECTS $OBJDIR/$objName"
          continue
        fi
        # the declaration names an object the gate has not built (cold, or a fresh include
        # set) -- build the GROUP once so the audit reads exactly what it names
        if [ "$groupBuilt" = no ]; then
          keptObjects=$OBJECTS
          if ! BuildGroup "$group"; then
            printf 'AUDIT %s does not compile %s under its own declaration\n' "$suiteDir" "$group"
            stragglers=refused
            break
          fi
          groupBuilt=yes
          OBJECTS=$keptObjects
        fi
        if [ -f "$OBJDIR/$objName" ]; then
          OBJECTS="$OBJECTS $OBJDIR/$objName"
        else
          printf 'AUDIT %s built %s and still has no %s\n' "$suiteDir" "$group" "$objName"
          stragglers=refused
          break
        fi
      done
      [ "$stragglers" = refused ] && break
    done
    if [ "$stragglers" = refused ]; then
      bad=1
      continue
    fi
    if [ -z "$OBJECTS" ]; then
      printf 'AUDIT %s resolved no objects at all -- an empty closure proves nothing\n' "$suiteDir"
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

OBJECTS=""
BuildGroup src/core/Json.cpp || Die "the prune's reader did not build"
pruneStale=no
[ -x "$BUILD/prune" ] || pruneStale=yes
[ "test/harness/shared/Prune.cpp" -nt "$BUILD/prune" ] && pruneStale=yes
for pruneObject in $OBJECTS; do
  [ "$pruneObject" -nt "$BUILD/prune" ] && pruneStale=yes
done
if [ "$pruneStale" = yes ]; then
  $CXX test/harness/shared/Prune.cpp $OBJECTS $CXXSTD $OPT $WARN -Itest/harness/shared -Itest/harness/shared/render -Isrc/core -o "$BUILD/prune" ||
    Die "the prune did not build"
fi
PRUNE_MARKER=$BUILD/prune.marker


TREES=test
[ -z "$SUITES" ] && TREES="test tools apps"
for named in $SUITES; do
  case "$named" in
    tools | tools/*) TREES="test tools" ;;
    apps | apps/*) TREES="test apps" ;;
  esac
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

# board:1801: the runner is the authority on what a PROOF is -- a translation unit it builds
# and runs as a case. A walk that decided by file CONTENT instead let a comment spell the
# needle that exempts its own file. Printed here, before any suite selection narrows the list
# and before the library is built, so asking the question costs nothing.
if [ "$CASELIST" = 1 ]; then
  for one in $TESTS; do printf '%s\n' "$one"; done
  exit 0
fi

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
# rule, never per edit. kFastGateBoundMs is MEASURED on this machine over the RUN population
# alone -- the build phases stand BESIDE the bound (board:1735). Re-derived 2026-08-23 over
# the population the gate ACTUALLY runs in (board:1749): 205 arms including the sanitised
# hostile-parser layers (board:1743), warm, WITH the hourly reviewer's own worktree gate
# building beside it -- the concurrent-nest case this tree mandates every hour, not an idle
# machine. Measured: 100.1 / 104.1 / 153.9 s of run, worst 153.9 -> the bound is that worst
# measurement times 1.5 for the machine's own weather = 230000 ms. A cold rebuild does not
# reach it either, because builds stand beside it.
NAMED_ONLY="harness/render render/outshine/drive render/outshine/frame render/outshine/scenario render/outshine/shader render/outshine/world tools apps"
FAST_GATE=no
kFastGateBoundMs=230000
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
  TESTS_ALL=$TESTS
  TESTS=$kept
  FAST_GATE=yes
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
builtSpentMs=0

# board:1797: the corpus lives in the system temp dir, because artefacts never live in the tree,
# and the machine sweeps that directory -- it took outshine-prepared and outshine-content
# between two gate runs. A gate that silently loses a third of its subjects is not a gate, so a
# case whose prepared input is gone is REBUILT before it is judged, from its own manifest and
# nothing else. Per case, never wholesale: `prepare.py all --every-case` renders the entire
# oracle ladder, which is 256 MB for a single animation case and hundreds of gigabytes for the
# tree -- the sporadic full proof's corpus, not this run's. The cost stands beside the bound
# with the builds, because a rebuild is a build.
# board:1797: a case can consume ANOTHER case's prepared product -- the glTF unit twins read
# test-render-outshine-grown-trs-hierarchy/scene.glb -- so the owner of a missing input is not
# knowable from the case that misses it. It IS knowable from the path the case names: the
# prepared directory is the owning manifest's own path with the slashes turned to dashes, and
# that mapping inverts by walking the manifests the tree declares.
RebuildOwner() {
  ownerMap=$BUILD/log/manifest-owners
  if [ ! -s "$ownerMap" ]; then
    find test -name manifest.json -print0 |
      while IFS= read -r -d '' candidate; do
        holder=${candidate%/manifest.json}
        printf '%s\t%s\n' "$(printf '%s' "$holder" | tr / -)" "$holder"
      done > "$ownerMap"
  fi
  ownerNames=$BUILD/log/owners
  # BSD sed has no \| alternation in a basic regex, so the two words are two expressions.
  sed -n -e 's|^UNPREPARED .*'"$PREPARED"'/\([^/]*\)/.*|\1|p' \
         -e 's|^REFUSED .*'"$PREPARED"'/\([^/]*\)/.*|\1|p' "$1" | sort -u > "$ownerNames"
  [ -s "$ownerNames" ] || return 1
  ownerRebuilt=no
  while IFS= read -r ownerDir; do
    [ -n "$(ls -A "$PREPARED/$ownerDir" 2>/dev/null | grep -v '^manifest.json$')" ] && continue
    holder=$(awk -F'\t' -v want="$ownerDir" '$1 == want { print $2; exit }' "$ownerMap")
    if [ -z "$holder" ]; then
      # not every prepared directory has a manifest behind it: the scenario assets are placed
      # from licensed copies against a digest the scenario pins, by their own subcommand.
      printf 'run.sh: %s has no prepared input and no manifest owns it -- placing the scenario assets (board:1778)\n' \
        "$ownerDir" >&2
      before=$(Now)
      if python3 test/harness/shared/corpus/prepare.py scenario-assets \
           > "$BUILD/log/scenario-assets-rebuild.log" 2>&1; then
        [ -n "$(ls -A "$PREPARED/$ownerDir" 2>/dev/null)" ] && ownerRebuilt=yes
      fi
      builtSpentMs=$(( builtSpentMs + $(Now) - before ))
      continue
    fi
    RebuildCase "$holder" "$PREPARED/$ownerDir"
    [ "$REBUILT" = yes ] && ownerRebuilt=yes
  done < "$ownerNames"
  [ "$ownerRebuilt" = yes ] || return 1
  return 0
}

RebuildCase() {
  REBUILT=no
  [ -f "$1/manifest.json" ] || return 0
  [ -z "$(ls -A "$2" 2>/dev/null | grep -v '^manifest.json$')" ] || return 0
  printf 'run.sh: %s has no prepared input -- rebuilding it from its manifest (board:1797)\n' \
    "${1#test/}" >&2
  before=$(Now)
  if python3 test/harness/shared/corpus/prepare.py all --manifest "$1/manifest.json" \
       > "$BUILD/log/$(printf '%s' "${1#test/}" | tr / -)-rebuild.log" 2>&1; then
    printf 'run.sh: rebuilt %s in %s ms\n' "${1#test/}" "$(( $(Now) - before ))" >&2
    REBUILT=yes
    ClaimCorpus
  else
    printf 'run.sh: the rebuild of %s FAILED -- %s\n' "${1#test/}" \
      "$BUILD/log/$(printf '%s' "${1#test/}" | tr / -)-rebuild.log" >&2
  fi
  builtSpentMs=$(( builtSpentMs + $(Now) - before ))
  return 0
}

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
partialCases=0
PARTIAL_SAID=""
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

# the nest carries a per-checkout identity and the corpus does not, so a second runner in a
# second checkout shares the 26 GB -- and pruning it is a DELETE. Sharing the bytes is worth
# keeping; sharing the right to remove them is not, so a runner that does not hold the
# corpus claim reads and never prunes (board:1789).
PruneCase() {
  [ "$CORPUSLOCK_MINE" = yes ] || return 0
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
    if [ "$TRAILER_PARTIAL" -gt 0 ]; then
      partialCases=$((partialCases + 1))
      PARTIAL_SAID="$PARTIAL_SAID $judgeId"
    fi
    expected=0
    { [ "$failures" -eq 0 ] && [ "$unpreparedHere" -eq 0 ]; } || expected=1
    [ "$status" -eq "$expected" ] ||
      Die "$judgeId reported FAILURES $failures UNPREPARED $unpreparedHere and exited $status, which do not agree: the reporter's answer was discarded, altered, or never returned -- $log"
    if [ "$failures" -gt 0 ] || [ "$unpreparedHere" -gt 0 ]; then
      # a case can miss more than one subject, so the retry repeats while a rebuild actually
      # produces something. It terminates: RebuildCase declines a prepared directory that is
      # already filled, so no owner is ever rebuilt twice in one case (board:1797).
      if RebuildOwner "$log"; then
        Judge "$judgeId" "$judgeBinary" "$judgeArgument" yes
        return 0
      fi
    fi
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

# the BINARY takes the object's own treatment (board:1751): a stamp of (mtime, size, name)
# over the test source, its extra sources, every prerequisite the linker's .d names and
# every object linked in -- compared for EQUALITY, because a checkout or a stash pop moves
# an mtime BACKWARDS and "-nt" then calls a stale binary current. The .cmd file covers the
# command line; this covers what the command line was fed.
# board:1811: the case source and its layer's extra sources compile in ONE command with one
# -MF, so the dependency file describes the LAST translation unit and says nothing about the
# case itself. Every case includes the harness directly, and a harness change that did not
# alter the trailer format would have left a stale binary running with the gate trusting it.
# The harness headers are stamped by name until the build gives each source its own .d.
BinaryStamp() {
  stampBinary=$1
  shift
  stampFiles="$* $(ls test/harness/shared/*.h test/harness/shared/render/*.h 2>/dev/null)"
  if [ -f "$stampBinary.d" ]; then
    for stampNeed in $(tr '\\' ' ' <"$stampBinary.d" | tr ':' ' ' | tr -s ' \n' ' '); do
      case "$stampNeed" in *.o|*.cpp|*.h|*.hpp) ;; *) continue ;; esac
      stampFiles="$stampFiles $stampNeed"
    done
  fi
  stat -f '%m|%z|%N' -- $stampFiles $OBJECTS 2>/dev/null || return 1
}

Fresh() {
  freshBinary=$1
  freshCommand=$2
  shift 2
  [ -f "$freshBinary" ] || return 1
  [ -f "$freshBinary.cmd" ] || return 1
  [ "$(cat "$freshBinary.cmd")" = "$freshCommand" ] || return 1
  [ -f "$freshBinary.d" ] || return 1
  [ -f "$freshBinary.stamp" ] || return 1
  freshStamp=$(BinaryStamp "$freshBinary" "$@") || return 1
  [ "$freshStamp" = "$(cat "$freshBinary.stamp")" ] || return 1
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
      KILLED_BY_TIME="$KILLED_BY_TIME $recordId"
      printf 'run.sh: %s was killed after %s s\n' "$recordId" "$TIMEOUT_S" >&2
      ;;
    SIGNAL)
      signalled=$((signalled + 1))
      printf 'run.sh: %s died on SIG%s\n' "$recordId" "$(kill -l $((status - 128)) 2>/dev/null)" >&2
      ;;
    BUILD) unbuilt=$((unbuilt + 1)) ;;
    UNPREP)
      unprepared=$((unprepared + 1))
      printf 'run.sh: %s has no prepared input and its rebuild did not give it one -- %s\n' \
        "$recordId" "$BUILD/log/$(printf '%s' "$recordId" | tr / -)-rebuild.log" >&2
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
      $CXX "$testSource" $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN -Itest/harness/shared -Itest/harness/shared/render $includes "$compileDefine" $linkage -MMD -MP -MF "$plainBinary.d" -o "$plainBinary" >>"$log" 2>&1 && printf '%s' "$buildCommand" >"$plainBinary.cmd" && BinaryStamp "$plainBinary" "$testSource" $(LayerExtraSources "$layer") >"$plainBinary.stamp" || built=no
    fi
  fi

  builtSpentMs=$(( builtSpentMs + $(Now) - before ))
  if [ "$built" = no ]; then
    failures=0
    skips=0
    verdict=BUILD
    Record "$id" "$(( $(Now) - before ))"
    continue
  fi

  # board:1823: a case that shells out to a builder spends the per-test budget COMPILING on a
  # cold tree, and the architect's mandated worktree is always cold. A .warms file beside a
  # case names what must stand built before it runs; the runner does that, and the time lands
  # in the build column where every other compile does.
  warms=${testSource%.cpp}.warms
  warmed=yes
  if [ -f "$warms" ]; then
    before=$(Now)
    while IFS= read -r warming; do
      [ -n "$warming" ] || continue
      ( eval "$warming" ) >>"$log" 2>&1 || warmed=no
    done <"$warms"
    builtSpentMs=$(( builtSpentMs + $(Now) - before ))
    if [ "$warmed" = no ]; then
      printf 'run.sh: %s declares a warm-up that did not succeed, so the case would have run without what it names\n' "$id" >&2
      failures=0
      skips=0
      verdict=BUILD
      Record "$id" "$(( $(Now) - before ))"
      continue
    fi
  fi

  sanitiser=$(LayerSanitiser "$layer")
  for exempt in $SANITISER_EXEMPT; do
    [ "$id" = "$exempt" ] && sanitiser=""
  done
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
      $CXX "$testSource" $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $SAN -Itest/harness/shared -Itest/harness/shared/render $includes "$compileDefine" $linkage -MMD -MP -MF "$sanitisedBinary.d" -o "$sanitisedBinary" >>"$sanitisedLog" 2>&1 && printf '%s' "$buildCommand" >"$sanitisedBinary.cmd" && BinaryStamp "$sanitisedBinary" "$testSource" $(LayerExtraSources "$layer") >"$sanitisedBinary.stamp" || built=no
    fi
    fi
    OBJDIR=$BUILD/obj
    SAN=""
    builtSpentMs=$(( builtSpentMs + $(Now) - before ))
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
      $CXX "$testSource" $(LayerExtraSources "$layer") $OBJECTS $toolchain $OPT $WARN $validation -Itest/harness/shared -Itest/harness/shared/render $includes "$compileDefine" $linkage -MMD -MP -MF "$validatedBinary.d" -o "$validatedBinary" >>"$validatedLog" 2>&1 && printf '%s' "$buildCommand" >"$validatedBinary.cmd" && BinaryStamp "$validatedBinary" "$testSource" $(LayerExtraSources "$layer") >"$validatedBinary.stamp" || built=no
    fi
    fi
    OBJDIR=$BUILD/obj
    EXTRA_DEFINES=""
    builtSpentMs=$(( builtSpentMs + $(Now) - before ))
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
    RebuildCase "$oneCase" "$preparedCase"
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
if [ "$FAST_GATE" = yes ]; then
  EverySourceStillCompiles || compileBlind=1
  WhatNoCorpusJudges
fi
# board:1810: a case that judged part of its subject may not read as one that judged all of it.
# The share is in the case's own PARTIAL line and the truncation point moves with the machine,
# so the trailer carries both the case and the share it reached.
for said in $PARTIAL_SAID; do
  partialLog=$BUILD/log/$(printf '%s' "$said" | tr / -).log
  partialShare=$(sed -n 's/^PARTIAL \([0-9.]*\) .*/\1/p' "$partialLog" | head -1)
  partialOf=$(sed -n 's/^PARTIAL [0-9.]* //p' "$partialLog" | head -1)
  printf 'run.sh: %s JUDGED PART OF ITS SUBJECT -- %s of %s, so this trailer says nothing about the rest and a run on another machine will stop somewhere else (board:1810)\n' \
    "$said" "$partialShare" "$partialOf" >&2
done

for killed in $KILLED_BY_TIME; do
  printf 'run.sh: %s MEASURED NOTHING -- it was killed at %s s before it finished, so its verdict is neither pass nor fail but absent, and a case that cannot finish is a case nobody runs (board:1778)\n' \
    "$killed" "$TIMEOUT_S" >&2
done
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
[ "$inverted" -gt 0 ] &&
  printf 'declared to fail and did, so the verdict stands inverted: %s\n' "$EXPECT_FAIL"

[ "$prunedCases" -gt 0 ] && printf \
  'test corpora: peak %s MB, %s MB after the last prune -- %s cases pruned, %s files and %s MB declined, %s file(s) left standing (each case: %s/*-prune.log)\n' \
  "$((peakKib / 1024))" "$((endKib / 1024))" "$prunedCases" "$prunedFiles" "$((prunedKib / 1024))" \
  "$stayedFiles" "$BUILD/log"

# the bound holds the RUN: compiling is the nest's business, and a cold rebuild after a
# header edit is not a slow test -- the measured span excludes the build phases, which are
# printed beside it so a bloating build is still visible (board:1735)
gateRunMs=$(( elapsedMs - builtSpentMs ))
if [ "$FAST_GATE" = yes ] && [ "$gateRunMs" -le "$kFastGateBoundMs" ]; then
  printf 'run.sh: gate headroom %s ms of %s (run %s ms, builds %s ms beside the bound)\n' \
    "$((kFastGateBoundMs - gateRunMs))" "$kFastGateBoundMs" "$gateRunMs" "$builtSpentMs"
fi
if [ "$FAST_GATE" = yes ] && [ "$gateRunMs" -gt "$kFastGateBoundMs" ]; then
  printf 'run.sh: THE FAST GATE OVERRAN ITS BOUND -- %s ms of RUN over the declared %s ms (builds %s ms stood beside the bound): a slow test is a finding, exactly like a slow frame (board:1601, 1735)\n' \
    "$gateRunMs" "$kFastGateBoundMs" "$builtSpentMs" >&2
  exit 1
fi

red=$((failed + timedout + signalled + unbuilt + undeclaredSkips + unprepared + compileBlind))
[ "$red" -eq 0 ] || exit 1
exit 0
