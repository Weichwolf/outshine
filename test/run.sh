#!/bin/sh

set -u
set -m
AUDIT=0
AUDIT_LAYERS=0
AUDIT_NUMBERS=0
AUDIT_ACCESS=0
STATE=0
STRANDED=0
DECIDED=126
PROTECTED=7
OPENDATA=36
INHERITS=0
OWNER_MAP_BUILT=no
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
# board:1839: prepare.py writes the ownership marker board:1789 scopes deletion by, and it must
# write the SAME identity run.sh compares against
export OUTSHINE_CORPUS_OWNER="$NEST"
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
# board:1885: the glTF-Validator corpus is scored against Khronos's own report, and 73 of its 263
# cases are RED with a reason that is not the reader being wrong about the SPEC -- 54 of them
# refuse with "no default scene to draw" for a fragment glTF 2.0 permits. They are declared here
# with their count so the day one of them stands, the gate says so.
EXPECT_FAIL="harness/claims/ExpectFail:1 khronos/validator/accessor-custom-property:1\
  khronos/validator/accessor-data-get-elements-matrix:1\
  khronos/validator/accessor-data-get-elements-sparse:1 khronos/validator/accessor-data-get-elements:1\
  khronos/validator/accessor-unknown-type:1 khronos/validator/accessor-valid:1\
  khronos/validator/animation-custom-property:1 khronos/validator/animation-valid:1\
  khronos/validator/asset-2-1-version:1 khronos/validator/asset-custom-property:1\
  khronos/validator/asset-min-version-valid:1 khronos/validator/asset-valid:1\
  khronos/validator/buffer-custom-property:1 khronos/validator/buffer-data-uri:1\
  khronos/validator/buffer-extra-padding:1 khronos/validator/buffer-non-relative-uri:1\
  khronos/validator/buffer-valid-2:1 khronos/validator/buffer-valid-placeholder:1 khronos/validator/buffer-valid:1\
  khronos/validator/buffer-view-custom-property:1 khronos/validator/buffer-view-valid:1\
  khronos/validator/camera-custom-property:1 khronos/validator/camera-perspective-yfov:1\
  khronos/validator/camera-valid:1 khronos/validator/glb-extra-data:1 khronos/validator/glb-two-images:1\
  khronos/validator/glb-valid-buffer:1 khronos/validator/glb-valid:1 khronos/validator/image-custom-property:1\
  khronos/validator/image-data-uri:1 khronos/validator/image-npot:1 khronos/validator/image-unrecognized-format:1\
  khronos/validator/image-valid-2:1 khronos/validator/image-valid:1\
  khronos/validator/json-integer-written-as-float:1 khronos/validator/material-alpha-modes:1\
  khronos/validator/material-custom-property:1 khronos/validator/material-empty-object:1\
  khronos/validator/material-multiple-extensions:1 khronos/validator/material-valid:1\
  khronos/validator/mesh-custom-property:1 khronos/validator/mesh-data-index-buffer-degenerate-triangle:1\
  khronos/validator/mesh-invalid-tangent:1 khronos/validator/mesh-primitive-generated-tangent-space:1\
  khronos/validator/mesh-primitive-incompatible-mode:1 khronos/validator/mesh-primitive-no-position:1\
  khronos/validator/mesh-valid:1 khronos/validator/node-custom-property:1 khronos/validator/node-matrix-default:1\
  khronos/validator/node-node-empty:1 khronos/validator/node-node-skinned-mesh-without-skin:1\
  khronos/validator/node-node-weights-override:1 khronos/validator/node-valid:1\
  khronos/validator/root-custom-property-escaped-name:1 khronos/validator/root-custom-property:1\
  khronos/validator/root-extras-non-object:1 khronos/validator/root-invalid-extension-name:1\
  khronos/validator/root-named-objects:1 khronos/validator/root-unused-objects:1 khronos/validator/root-valid:1\
  khronos/validator/sampler-custom-property:1 khronos/validator/sampler-empty-object:1\
  khronos/validator/sampler-valid:1 khronos/validator/scene-custom-property:1 khronos/validator/scene-valid:1\
  khronos/validator/skin-custom-property:1 khronos/validator/skin-ignored-animated-transform:1\
  khronos/validator/skin-ignored-local-transform:1 khronos/validator/skin-ignored-parent-transform:1\
  khronos/validator/skin-valid:1 khronos/validator/texture-custom-property:1\
  khronos/validator/texture-empty-object:1 khronos/validator/texture-valid:1"

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
    --audit-layers) AUDIT_LAYERS=1; shift ;;
    --audit-numbers) AUDIT_NUMBERS=1; shift ;;
    --audit-access) AUDIT_ACCESS=1; shift ;;
    --state) STATE=1; shift ;;
    -*) Die "unknown option '$1'" ;;
    *) SUITES="$SUITES ${1%/}"; SUITE=${1%/}; shift; continue ;;
  esac
done

LayerIncludes() {
  case "$1" in
    harness/claims) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -Isrc/world/ground -Isrc/generators -Isrc/generators/base " ;;
    harness/geographiclib/geodesic) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -Isrc/world/ground -Isrc/generators -Isrc/generators/base -Isrc/world/data -Itest/harness/shared" ;;
    outshine/scenario) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/format -Isrc/base/spatial -Isrc/scenario -Itest/harness/shared" ;;
    outshine/content) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/base/io -Isrc/content/gltf -Isrc/content/shade -Itest/harness/shared" ;;
    outshine/geo) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/base/io -Isrc/world/data -Isrc/world/ground -Isrc/world/ground/tiles -Isrc/compositor -Isrc/content/gltf -Isrc/content/shade -Itest/harness/shared -Isrc/generators -Isrc/generators/base -Isrc/actor/path" ;;
    outshine/fuzz) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -Isrc/world/ground -Isrc/generators -Isrc/generators/base -Isrc/content/gltf -Itest/harness/shared" ;;
    outshine/physics) printf '%s' "-Iinclude -Isrc/base -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -Isrc/world/ground -Isrc/generators -Isrc/generators/base -Isrc/actor/body -Isrc/actor/path -Isrc/actor/mind -Isrc/sim -Itest/harness/shared" ;;
    outshine/audio) printf '%s' "-Iinclude -Isrc/audio -Isrc/base/math" ;;
    outshine/door) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -Isrc/world/ground -Isrc/generators -Isrc/generators/base -Isrc/content/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/device -Isrc/render/stages -Isrc/compositor -Isrc/world/data -Isrc/scene -Isrc/scenario -Isrc/ui -Isrc/host -Isrc/engine -Itest/harness/shared" ;;
    harness/khronos/validator) printf '%s' "-Iinclude -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -Isrc/world/ground -Isrc/generators -Isrc/generators/base -Itest/harness/shared" ;;
    harness/khronos/glTF | harness/khronos/generator | outshine/grown) printf '%s' "-Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -Isrc/world/ground -Isrc/generators -Isrc/generators/base -Isrc/content/gltf -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/device -Isrc/render/stages -Isrc/compositor -Isrc/world/data -Isrc/scene -Isrc/scenario -Isrc/ui -Iinclude -Isrc/host -Isrc/engine" ;;
    harness/wpt/css) printf '%s' "-Iinclude -Isrc/base/format -Isrc/base/math -Isrc/base/io -Isrc/base/spatial -Isrc/content/shade -Isrc/content/gltf -Isrc/render/draw -Isrc/ui -Itest/harness/shared" ;;
    harness/test262/js) printf '%s' "-Iinclude -Isrc/base/format -Itest/harness/shared" ;;
    apps/viewer/src) printf '%s' "-Iinclude -Iapps/viewer/src/parts" ;;
    apps/bench/src) printf '%s' "-Iinclude" ;;
    apps/demo/src | apps/driver/src) printf '%s' "-Iinclude" ;;
    *) return 1 ;;
  esac
}

LayerToolchain() {
  case "$1" in
    harness/khronos/glTF | harness/khronos/generator | outshine/grown | outshine/frame | apps/viewer/src | outshine/door | harness/wpt/css) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    apps/demo/src | apps/driver/src | apps/bench/src) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    harness/geographiclib/geodesic | harness/khronos/validator) printf '%s' "$CXXSTD $(pkg-config --cflags sdl3) $(pkg-config --cflags sdl3-image)" ;;
    *) printf '%s' "$CXXSTD" ;;
  esac
}
# ONE case is exempt, by name and with its reason: the heap ledger measures the tree's own
# operator new, and ASan REPLACES that allocator -- sanitising it would measure ASan, not
# the instrument. The exemption is a case, never a layer (board:1743).
SANITISER_EXEMPT="unit/core/EveryByteTheHeapTakesLandsUnderATagOrUnderOther"

LayerSanitiser() {
  case "$1" in
    harness/khronos/glTF | harness/khronos/generator | outshine/grown | outshine/shader | unit/ui | unit/core | unit/gltf | unit/data | unit/ground | unit/ground/tiles) printf '%s' "-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1" ;;
    *) printf '%s' "" ;;
  esac
}

LayerValidation() {
  case "$1" in
    harness/khronos/glTF | harness/khronos/generator | outshine/grown | outshine/shader) printf '%s' "-DOUTSHINE_GPU_VALIDATION=1" ;;
    *) printf '%s' "" ;;
  esac
}

LayerLink() {
  case "$1" in
    outshine/fuzz | outshine/geo | outshine/content) printf '%s' "-lz" ;;
    harness/khronos/glTF | harness/khronos/generator | outshine/grown | outshine/frame | apps/viewer/src | outshine/door | outshine/client | harness/wpt/css) printf '%s' "$(pkg-config --libs sdl3) $(pkg-config --libs sdl3-image) $(pkg-config --libs sdl3-ttf) -lz -lcurl" ;;
    harness/claims) printf '%s' "-lz" ;;
    apps/demo/src | apps/driver/src | apps/bench/src) printf '%s' "$(pkg-config --libs sdl3) $(pkg-config --libs sdl3-image) $(pkg-config --libs sdl3-ttf) -lz -lcurl" ;;
    harness/geographiclib/geodesic | harness/khronos/validator) printf '%s' "$(pkg-config --libs sdl3) $(pkg-config --libs sdl3-image) $(pkg-config --libs sdl3-ttf) -lz -lcurl" ;;
    *) printf '%s' "" ;;
  esac
}

LayerGroups() {
  case "$1" in
    harness/wpt/css) printf '%s' "src/base/format/Json.cpp src/ui" ;;
    harness/test262/js) printf '%s' "src/base/format/Json.cpp src/base/format/Script.cpp" ;;
    harness/claims) printf '%s' "src/base/format/Sha256.cpp src/base/format/Json.cpp" ;;
    outshine/scenario) printf '%s' "src/base/math src/base/format src/base/spatial src/scenario/Triggers.cpp src/scenario/ScenarioRead.cpp src/scenario/Tables.cpp" ;;
    outshine/content) printf '%s' "src/base src/base/math src/base/geo src/base/format src/base/spatial src/base/io src/content/gltf" ;;
    outshine/geo) printf '%s' "src/base src/base/math src/base/geo src/base/format src/base/spatial src/base/io src/world/data src/world/ground/tiles src/world/ground src/compositor src/content/gltf src/content/shade src/generators/base src/generators src/generators/draw" ;;
    outshine/fuzz) printf '%s' "src/base src/base/math src/base/geo src/base/format src/base/spatial src/content/shade src/world/weather src/world/sky src/base/io src/content/gltf" ;;
    outshine/physics) printf '%s' "src/base src/base/math src/base/geo src/base/spatial src/actor/body src/actor/path src/actor/mind src/sim/Rigging.cpp src/sim/DriveTick.cpp" ;;
    outshine/audio) printf '%s' "src/audio src/base/math" ;;
    outshine/door) printf '%s' "src/base src/base/math src/base/geo src/base/format src/base/spatial src/content/shade src/world/weather src/world/sky src/base/io src/content/gltf src/render/plan src/render/draw src/render src/render/device src/render/stages src/scene src/ui src/world/data src/world/ground src/world/ground/tiles src/generators/base src/generators src/generators/draw src/sim src/actor/path src/actor/body src/actor/mind src/host src/engine/Image.cpp src/engine/Surfaces.cpp src/compositor src/engine/Asset.cpp src/engine/Overlay.cpp src/engine/Live.cpp src/engine/Picturing.cpp src/engine/Declaring.cpp src/engine/Keeping.cpp src/engine/Advancing.cpp src/engine/Engine.cpp src/audio src/scenario/Tables.cpp src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/scenario/Views.cpp src/scenario/InputMap.cpp src/scenario/Triggers.cpp src/engine/InputPump.cpp src/engine/Assembly.cpp" ;;
    harness/khronos/glTF | harness/khronos/generator | outshine/grown | outshine/frame | apps/viewer/src | outshine/client | harness/geographiclib/geodesic | harness/khronos/validator) printf '%s' "src/base src/base/math src/base/geo src/base/format src/base/spatial src/content/shade src/world/weather src/world/sky src/base/io src/content/gltf src/render/plan src/render/draw src/render src/render/device src/render/stages src/scene src/ui src/world/data src/world/ground src/world/ground/tiles src/generators/base src/generators src/generators/draw src/sim src/actor/path src/actor/body src/actor/mind src/host src/engine/Image.cpp src/engine/Surfaces.cpp src/compositor src/engine/Asset.cpp src/engine/Overlay.cpp src/engine/Live.cpp src/engine/Picturing.cpp src/engine/Declaring.cpp src/engine/Keeping.cpp src/engine/Advancing.cpp src/engine/Engine.cpp src/audio src/scenario/Tables.cpp src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/scenario/Views.cpp src/scenario/InputMap.cpp src/scenario/Triggers.cpp src/engine/InputPump.cpp src/engine/Assembly.cpp" ;;
    apps/demo/src | apps/driver/src | apps/bench/src) printf '%s' "src/base src/base/math src/base/geo src/base/format src/base/spatial src/content/shade src/world/weather src/world/sky src/base/io src/content/gltf src/render/plan src/render/draw src/render src/render/device src/render/stages src/ui src/actor/path src/actor/body src/actor/mind src/world/data src/world/ground/tiles src/world/ground src/engine/Image.cpp src/engine/Surfaces.cpp src/engine/Asset.cpp src/engine/Overlay.cpp src/engine/Live.cpp src/engine/Picturing.cpp src/engine/Declaring.cpp src/engine/Keeping.cpp src/engine/Advancing.cpp src/audio src/scenario/Tables.cpp src/scenario/ScenarioRead.cpp src/scenario/ScenarioLayer.cpp src/scenario/Views.cpp src/scenario/InputMap.cpp src/scenario/Triggers.cpp src/engine/InputPump.cpp src/host src/compositor src/generators/base src/generators src/generators/draw src/sim src/scene src/engine/Assembly.cpp" ;;
    *) return 1 ;;
  esac
}

LayerCases() {
  case "$1" in
    harness/khronos/glTF) find test/khronos/glTF -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/khronos/generator) find test/khronos/generator -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    outshine/grown) find test/outshine/grown -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/wpt/css) find test/wpt/css -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/test262/js) find test/test262/js -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/khronos/validator) find test/khronos/validator -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    harness/geographiclib/geodesic) find test/geographiclib/geodesic -name manifest.json | sed -e 's|/manifest.json$||' | sort ;;
    *) printf '%s' "" ;;
  esac
}

# board:1860: a PROGRAM under apps/ is not a case and not a harness -- it is the thing the
# library exists for, and it must be BUILT so an entry point cannot break in silence. The gate
# links it and runs it with --help, which every program answers without touching the world.
Programs() {
  case "$1" in
    apps/demo/src) printf '%s' "the demo's entry point" ;;
    apps/driver/src) printf '%s' "the driver's entry point" ;;
    apps/bench/src) printf '%s' "the bench's entry point" ;;
    apps/viewer/src) printf '%s' "the viewer's entry point" ;;
    *) return 1 ;;
  esac
}

NotTheHarnesses() {
  case "$1" in
    harness/shared | harness/khronos/glTF | harness/khronos/generator | outshine/grown) printf '%s' "the harness's own clock and its prune, run by this script and judged by nobody" ;;
    harness/shared/render) printf '%s' "the render scoring instrument, compiled into each corpus's own harness" ;;
    harness/shared/frame) printf '%s' "the linker's own walks and the program that links the generator archive alone -- a claim compiles it, so the harness does not" ;;
    apps/viewer/src/parts) printf '%s' "the browser's own declaration and its face, compiled into the browser" ;;
    harness/khronos/glTF/prepare | harness/khronos/generator/prepare | outshine/grown/prepare | harness/wpt/css/prepare | harness/test262/js/prepare) printf '%s' "how a corpus is obtained, run by test/harness/shared/corpus/prepare.py and never by this script" ;;
    harness/shared/corpus | harness/shared/corpus/*) printf '%s' "the offline preparer's own, compiled and run by test/harness/shared/corpus/prepare.py" ;;
    *) return 1 ;;
  esac
}

LayerExtraSources() {
  case "$1" in
    apps/viewer/src) printf '%s' "apps/viewer/src/parts/Face.cpp" ;;
    harness/khronos/glTF | harness/khronos/generator | outshine/grown) printf '%s' "test/harness/shared/render/Parity.cpp" ;;
    *) printf '%s' "" ;;
  esac
}
# THE LAYERING IS A DIRECTION AND THE DIRECTION IS DECLARED. Unreal states each module's
# dependencies in its own Build.cs and the build enforces them; RAGE states them by library and
# by prefix. Here they are one table, and a source that includes across it refuses. Without this
# the layering is a convention, and a convention is how src/core became a drawer of 44 headers
# (board:1902).
# WHAT A TIER REACHES IS DECLARED BESIDE IT, which is Unreal's `Build.cs`: a module's dependency
# list sits IN the module, so adding a directory does not mean editing a table at the root that
# nobody thinks to look at. `src/<tier>/reaches` holds one line of tier names, empty for `base`.
# A directory with no `reaches` file is REFUSED rather than assumed to reach nothing -- silence
# and "nothing" are different claims, and the second one has to be written down.
LayerReaches() {
  case "$1" in */*|"") return 1 ;; esac
  [ -f "src/$1/reaches" ] || return 1
  printf '%s' "$(cat "src/$1/reaches")"
}


# THE INCLUDE SET IS THE TIER TABLE, DERIVED -- one spelling and not two. `LayerReaches` above
# states what each tier may reach; a source compiles with `include/`, its OWN tier's directories and
# the directories of the tiers its row names, and NOTHING else. So a cross-tier include fails at the
# `#include` with a file and a line, rather than compiling and being reported afterwards by
# `--audit-layers`. That is what Unreal spends `Build.cs` on: a module that did not declare a
# dependency cannot FIND the header, and the violation is unspellable rather than reported.
#
# This replaced thirty hand-kept arms whose first one bundled `src/base/math` with `content/shade`,
# `world/weather` and `world/sky` -- so `base/`, which reaches nothing, compiled with content and
# world on its path (board:1956). `--audit-layers` stays: an include path cannot see a CYCLE between
# two modules inside ONE tier, which is the case that needs it.
#
# SDL3 comes LAST on the path and our own directories first, because a system include directory that
# carries `png.h` shadows `src/base/io/Png.h` on a case-insensitive filesystem -- which is what the
# first version of this derivation did, and the compiler said so by name.
#
# SDL3 is on EVERY path because the door requires it: `include/Outshine.h` includes SDL3, so any
# translation unit that reaches the door reaches SDL3, and a per-group SDL arm was a third hand-kept
# map beside the two this replaced. `src/render/shaders` holds no headers and is left off every path.
GroupIncludes() {
  case "$1" in src/*) ;; *) return 1 ;; esac
  includeTier=$(printf '%s' "$1" | cut -d/ -f2)
  includeReaches=$(LayerReaches "$includeTier") || return 1
  includeSet="-Iinclude"
  for includeFrom in $includeTier $includeReaches; do
    for includeDir in $(find "src/$includeFrom" -type d | sort); do
      case "$includeDir" in */shaders) continue ;; esac
      includeSet="$includeSet -I$includeDir"
    done
  done
  includeSet="$includeSet $(pkg-config --cflags sdl3)"
  case "$1" in
    src/engine/Image.cpp | src/engine) includeSet="$includeSet $(pkg-config --cflags sdl3-image)" ;;
  esac
  printf '%s' "$includeSet"
}

GroupToolchain() {
  case "$1" in
    src/render | src/render/device | src/render/stages | src/render/Readback.cpp | src/engine/Surfaces.cpp | src/engine/Overlay.cpp | src/engine/Asset.cpp | src/engine/Picturing.cpp | src/engine/Declaring.cpp | src/engine/Keeping.cpp | src/engine/Advancing.cpp | src/engine/Live.cpp) LayerToolchain render ;;
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

# THE COMPILE LINE'S IDENTITY IS IN THE OBJECT'S NAME, so freshness only has to answer the other
# question: is anything this unit reads newer than what it produced. That is one `sed` to read the
# dependency list the compiler already wrote, and then shell builtins -- where the stamp it
# replaces spent a `stat` over a hundred paths and a `cat` per unit, 58 ms each over 158 units.
UpToDate() {
  objectPath=$1
  sourcePath=$2
  depsPath=${objectPath%.o}.d
  [ -f "$objectPath" ] || return 1
  [ -f "$depsPath" ] || return 1
  [ "$objectPath" -nt "$sourcePath" ] || return 1
  for readAlso in $(sed -e 's/^[^:]*://' -e 's/\\//g' "$depsPath"); do
    [ "$objectPath" -nt "$readAlso" ] || return 1
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
  # the id carries the WHOLE compile line: includes, std, optimisation and warnings --
  # editing -O2 or the warning set in this file silently reused objects built with the
  # old one, and two of five flag groups is not an identity (board:1751). It is a GROUP's
  # property, so it is computed once here rather than once per unit: at 157 units that was
  # three processes each for a value that could not differ between them.
  setId=$(printf '%s|%s|%s|%s' "$groupIncludes" "$groupStd" "$OPT" "$WARN" | cksum | cut -d' ' -f1)
  for unit in $groupUnits; do
    [ -e "$unit" ] || continue
    unitFolder=${unit%/*}
    unitStem=${unit##*/}
    unitObject=$OBJDIR/$(printf '%s' "$unitFolder" | tr / -)-${unitStem%.cpp}.$setId.o
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

# An ORACLE THAT CANNOT EXIST is a different thing from one nobody rendered, and until this
# declaration the gate could not say which it was looking at -- both arrived as UNPREPARED, in a
# count with no name and no expiry. Three cases cannot be prepared with this toolchain at all:
#
#   khronos/glTF/CubeVisibility    Blender's glTF importer: "Extension KHR_node_visibility is not
#   khronos/glTF/LightVisibility   available on this addon version". The oracle half of the case
#                                  cannot be rendered, so there is nothing to compare against.
#   khronos/glTF/AnimationPointerUVs  the same importer crashes reading KHR_animation_pointer over
#                                  KHR_texture_transform: KeyError: 'animations'.
#
# Each is named with its arm count, the same shape EXPECT_FAIL uses, so the day the importer
# grows the extension and a declared case PREPARES, the gate turns red on the stale line rather
# than going quietly green. board:1923.
EXPECT_UNPREPARED="khronos/glTF/CubeVisibility:3 khronos/glTF/LightVisibility:3\
  khronos/glTF/AnimationPointerUVs:3"

DeclaredUnprepared() {
  for declared in $EXPECT_UNPREPARED; do
    case "$1" in "${declared%:*}" | "${declared%:*}"~*)
      return 0
      ;;
    esac
  done
  return 1
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
    libraryGroup=${libraryUnit%/*}
    case "$libraryGroups" in *" $libraryGroup "*) continue ;; esac
    libraryGroups="$libraryGroups$libraryGroup "
    BuildGroup "$libraryGroup" || Die "the library group $libraryGroup did not build"
  done
  mkdir -p build
  archiveStale=yes
  if [ -f build/liboutshine.a ]; then
    archiveStale=no
    for member in $OBJECTS; do
      if [ "$member" -nt build/liboutshine.a ]; then archiveStale=yes; break; fi
    done
  fi
  if [ "$archiveStale" = yes ]; then
    rm -f build/liboutshine.a
    ar rcs build/liboutshine.a $OBJECTS || Die "the archive did not write"
  fi
  printf -- '-> build/liboutshine.a (%s objects)\n' "$(echo $OBJECTS | wc -w | tr -d ' ')"

  # THE GENERATORS AS THEIR OWN ARCHIVE, and the member list is DERIVED. Owner's target: another
  # project uses the generators alone. A claim that they WOULD link is weaker than an archive that
  # does, so `linkreach.sh` computes the closure the linker computes -- every object the generator
  # objects reach by undefined symbol -- and those members, and only those, are archived again.
  # A second hand-kept list would drift from the first the day somebody adds an include.
  reachedGenerators=$(sh test/harness/shared/frame/linkreach.sh build/liboutshine.a \
                        build/genwalk src-generators 2>/dev/null | sed 's|:$||')
  generatorObjects=""
  for reached in $reachedGenerators; do
    for candidate in $OBJECTS; do
      case "$candidate" in */"$reached") generatorObjects="$generatorObjects $candidate" ;; esac
    done
  done
  if [ -n "$generatorObjects" ]; then
    rm -f build/libgenerators.a
    ar rcs build/libgenerators.a $generatorObjects || Die "the generator archive did not write"
    printf -- '-> build/libgenerators.a (%s objects, closed over the linker'"'"'s own walk)\n' \
      "$(echo $generatorObjects | wc -w | tr -d ' ')"
  fi
  for program in $(find apps -name '*.cpp' | sort); do
    layer=$(dirname "$program")
    Programs "$layer" >/dev/null 2>&1 || continue
    named=build/outshine-$(basename "$(dirname "$layer")")
    if [ -f "$named" ] && [ "$named" -nt build/liboutshine.a ] && [ "$named" -nt "$program" ]; then
      continue
    fi
    $CXX $CXXSTD $(LayerToolchain "$layer") $WARN $(LayerIncludes "$layer") \
      "$program" $(LayerExtraSources "$layer") build/liboutshine.a $(LayerLink "$layer") \
      -o "$named" ||
      Die "$program does not build into $named"
    printf -- '-> %s\n' "$named"
  done
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

# A program that does not link is a red the gate can PRODUCE and could not DECLARE, so its
# verdict carried a nameless failure every run. `apps/viewer/src/main.cpp` calls
# `outshine::Viewer::Declaration` and `outshine::Viewer::StageRegion`, which live in
# `apps/viewer/src/parts/Face.cpp` -- a second translation unit of the client's own, so the
# client is NOT one entry point over the library and this check says so correctly. board:1898
# holds the repair: the viewer entire is 706 lines and its shape is about 100. Until it lands
# the red is declared here by name, and the day the viewer links from its entry point alone the
# gate turns red on this stale line rather than going quietly green.
EXPECT_UNLINKED="apps/viewer/src/main.cpp"

EveryProgramStillLinks() {
  built=0
  brokenPrograms=0
  staleUnlinked=0
  for one in $PROGRAMS; do
    layer=$(dirname "$one")
    declaredUnlinked=no
    for named in $EXPECT_UNLINKED; do
      [ "$named" = "$one" ] && declaredUnlinked=yes
    done
    if $CXX $CXXSTD $(LayerToolchain "$layer") $WARN $(LayerIncludes "$layer") -c "$one" \
         -o "$BUILD/program.o" \
         >"$BUILD/program.log" 2>&1 &&
       $CXX $CXXSTD "$BUILD/program.o" build/liboutshine.a $(LayerLink "$layer") \
         -o "$BUILD/program" >>"$BUILD/program.log" 2>&1 &&
       "$BUILD/program" --help >/dev/null 2>&1; then
      built=$((built + 1))
      if [ "$declaredUnlinked" = yes ]; then
        staleUnlinked=$((staleUnlinked + 1))
        printf 'run.sh: %s LINKS and EXPECT_UNLINKED still names it -- the declaration outlived what it declared, so take it out\n' "$one" >&2
      fi
    elif [ "$declaredUnlinked" = yes ]; then
      printf 'run.sh: %s does not link, and EXPECT_UNLINKED says so -- board:1898\n' "$one" >&2
    else
      brokenPrograms=$((brokenPrograms + 1))
      printf 'run.sh: %s does not BUILD AND ANSWER --help, and a program nobody links is an entry point that breaks in silence -- %s\n' "$one" "$BUILD/program.log" >&2
      head -6 "$BUILD/program.log" >&2
    fi
  done
  [ -n "$PROGRAMS" ] &&
    printf 'run.sh: %s program(s) build and answer --help, %s do not -- each on the include set LayerIncludes declares for it, which is the set make builds with (board:1584)\n' \
      "$built" "$brokenPrograms"
  [ "$brokenPrograms" -eq 0 ] && [ "$staleUnlinked" -eq 0 ]
}

WhatNoCorpusJudges() {
  for family in test/*/; do
    family=${family%/}
    [ "$family" = test/outshine ] && continue
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


# WHAT THE LIBRARY IS, ON ONE PAGE, GENERATED. There is no RFC for this and the nearest
# established shapes are a .pyi stub and a man page's SYNOPSIS: signatures without bodies. Those
# answer what the door OFFERS. This answers what the tree IS -- the door's verbs, the tier graph,
# what stands wider than private, what is declared red, and every item still open. Nothing here
# is written by hand, so nothing here can lie about the tree. It carries no list of what the
# cases CLAIM: a Covers line is an assertion, the page is generated without running a gate and
# cannot say whether one is green today, and an assertion nobody checked is the one thing a
# generated page must not carry.
StateDoor() {
  printf '\n## Door -- `include/`\n'
  for header in include/*.h; do
    printf '\n### `%s`\n\n' "${header#include/}"
    sed -n 's|^struct \([A-Za-z]*\) {|value: \1|p; s|^class \([A-Za-z]*\) {|type: \1|p' \
      "$header" | sed 's|^|    |'
    sed -n 's|^  \[\[nodiscard\]\] \(.*\);$|\1|p; s|^  \(void [A-Z].*\);$|\1|p' "$header" |
      sed 's|  *| |g' | sed 's|^|    |'
  done
  modules=$BUILD/log/module-of-header
  mkdir -p "$BUILD/log"
  find src -name '*.h' -not -path 'src/assets/*' |
    sed -e 's|^src/\([^/]*/[^/]*\)/.*/\([^/]*\)$|\2 \1|' \
        -e 's|^src/\([^/]*/[^/]*\)/\([^/]*\.h\)$|\2 \1|' \
        -e 's|^src/\([^/]*\)/\([^/]*\.h\)$|\2 \1|' | sort -u > "$modules"
}

StateShape() {
  printf '\n## Shape\n\nModule depends on module, derived from the includes themselves.\n\n```mermaid\nflowchart LR\n'
  awk -v owner="$modules" '
    BEGIN { while ((getline line < owner) > 0) { split(line, field, " "); owns[field[1]] = field[2] } }
    FNR == 1 {
      within = FILENAME
      sub(/^src\//, "", within)
      depth = split(within, part, "/")
      from = (depth >= 3) ? part[1] "/" part[2] : (depth == 2 ? part[1] : within)
      delete already
    }
    /^#include "/ {
      head = $0
      sub(/^#include "/, "", head); sub(/".*$/, "", head); sub(/^.*\//, "", head)
      if (head in already) { next }
      already[head] = 1
      to = owns[head]
      if (to == "" || to == from) { next }
      crosses[from "|" to]++
    }
    END { for (edge in crosses) { printf "%d %s\n", crosses[edge], edge } }
  ' $(find src \( -name '*.cpp' -o -name '*.h' \) -not -path 'src/assets/*' | sort) |
    sort -rn > "$BUILD/log/module-edges"
  awk '$1 >= 3' "$BUILD/log/module-edges" |
    while read -r many edge; do
      printf '  %s --> |%s| %s\n' "$(printf '%s' "$edge" | cut -d'|' -f1 | tr '/' '_')" "$many" \
        "$(printf '%s' "$edge" | cut -d'|' -f2 | tr '/' '_')"
    done
  printf '```\n'
  printf '  %s edge(s) drawn, %s thinner than three includes not drawn\n' \
    "$(awk '$1 >= 3' "$BUILD/log/module-edges" | wc -l | tr -d ' ')" \
    "$(awk '$1 < 3' "$BUILD/log/module-edges" | wc -l | tr -d ' ')"
  awk '{ seen[$2] = $1 }
       END { for (k in seen) { split(k, e, "|"); back = e[2] "|" e[1];
               if (back in seen && e[1] < e[2])
                 printf "  CYCLE %s and %s include each other, %s deep and %s back\n",
                        e[1], e[2], seen[k], seen[back] } }' "$BUILD/log/module-edges"
}

StateTiers() {

  printf '\n## Tiers\n\nWhat each directory under `src/` may include. `--audit-layers` refuses a crossing.\n\n| tier | reaches |\n|---|---|\n'
  for tier in base content world actor render scene scenario ui audio host compositor sim engine; do
    reaches=$(LayerReaches "$tier") || continue
    printf '| `%s` | %s |\n' "$tier" "${reaches:-nothing}"
  done
}

StateMass() {
  printf '\n## Mass\n\nThe heaviest files. Headers and sources counted apart.\n\n| lines | kind | file |\n|---|---|---|\n'
  : > "$BUILD/log/unit-mass"
  for kind in h cpp; do
    massFiles=$(find src -name "*.$kind" -not -path 'src/assets/*' | sort)
    [ -n "$massFiles" ] || continue
    wc -l $massFiles |
      awk -v kind="$kind" '$2 != "total" { sub(/^src\//, "", $2); printf "%s %s %s\n", $1, kind, $2 }' \
      >> "$BUILD/log/unit-mass"
  done
  sort -rn "$BUILD/log/unit-mass" -o "$BUILD/log/unit-mass"
  head -10 "$BUILD/log/unit-mass" | while read -r many kind one; do
    printf '| %s | `%s` | `%s` |\n' "$many" "$kind" "$one"
  done
  printf '| **%s** | `h` | *the median of %s header(s)* |\n' \
    "$(awk '$2 == "h" { n[++k] = $1 } END { print n[int(k/2)] }' "$BUILD/log/unit-mass")" \
    "$(awk '$2 == "h"' "$BUILD/log/unit-mass" | wc -l | tr -d ' ')"
  printf '| **%s** | `cpp` | *the median of %s source(s)* |\n' \
    "$(awk '$2 == "cpp" { n[++k] = $1 } END { print n[int(k/2)] }' "$BUILD/log/unit-mass")" \
    "$(awk '$2 == "cpp"' "$BUILD/log/unit-mass" | wc -l | tr -d ' ')"
}

StateCarpet() {

  printf '\n## Carpet\n\nThe widest public surfaces.\n\n| `[[nodiscard]]` | header |\n|---|---|\n'
  grep -c '\[\[nodiscard\]\]' $(find src include -name '*.h' -not -path 'src/assets/*' | sort) |
    awk -F: '{ printf "%s %s\n", $2, $1 }' | sort -rn | head -6 | while read -r many header; do
      printf '| %s | `%s` |\n' "$many" "$header"
    done
}

StateTwins() {

  printf '\n## Twins\n\nHeader names that collide.\n\n'
  twinHeaders=$(find src include -name '*.h' | sort)
  twinNames=$(printf '%s\n' "$twinHeaders" | sed 's|.*/||' | sort | uniq -d)
  if [ -n "$twinNames" ]; then
    printf '%s\n' "$twinNames" | while IFS= read -r name; do
      printf -- '- `%s` -- %s\n' "$name" \
        "$(printf '%s\n' "$twinHeaders" | grep "/$name\$" | tr '\n' ' ')"
    done
  else
    printf '  none -- and --audit-layers REFUSES the day one appears, because both walks on\n    this page resolve an include by its basename alone\n'
  fi
}

StateStranded() {

  printf '\n## Stranded\n\nSources no declared suite links, so nothing they hold is proven.\n\n'
  for suiteDir in $(find test apps -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u); do
    groups=$(LayerGroups "$suiteDir" 2>/dev/null) || continue
    for group in $groups; do
      if [ -d "$group" ]; then find "$group" -maxdepth 1 -name '*.cpp'; else printf '%s\n' "$group"; fi
    done
  done | sort -u > "$BUILD/log/state-linked"
  find src -name '*.cpp' -not -path 'src/assets/*' | sort > "$BUILD/log/state-carried"
  comm -23 "$BUILD/log/state-carried" "$BUILD/log/state-linked" | sed 's|^|  |'
}

StateAccess() {

  printf '\n## Access\n\nWhat stands wider than private. Private is the default and a wider door justifies itself.\n\n'
  printf '%s protected section(s), and inheritance is right where a stable interface carries\n' \
    "$(grep -rc '^ *protected:' src --include=*.h 2>/dev/null | awk -F: '{ n += $2 } END { print n + 0 }')"
  printf 'shared machinery -- Source, WebTileSource, TerrariumDem is that shape.\n\n'
  grep -rn '^ *protected:' src --include=*.h 2>/dev/null | sed -E 's|:[0-9]+:.*||' | sort -u |
    sed 's|^|- `|; s|$|`|'
  printf '\n%s public data member(s) in a class -- an invariant nobody can hold.\n\n| members | header |\n|---|---|\n' "$OPENDATA"
  awk '
    FNR == 1 { inclass = 0; access = "private" }
    /^[ \t]*class[ \t]+[A-Za-z_]/ { inclass = 1; access = "private"; next }
    /^[ \t]*struct[ \t]+[A-Za-z_]/ { inclass = 0; next }
    inclass == 0 { next }
    /^[ \t]*public:/ { access = "public"; next }
    /^[ \t]*(private|protected):/ { access = "private"; next }
    access == "public" && /^[ \t]+[A-Za-z_][A-Za-z_0-9:<>,& *]*[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]*(=|\[|;)/ \
      && !/\(/ && !/using/ && !/return/ { n[FILENAME]++ }
    END { for (f in n) printf "%5d  %s\n", n[f], f }' \
    $(find src -name '*.h' -not -path 'src/assets/*' | sort) | sort -rn |
    awk '{ printf "| %s | `%s` |\n", $1, $2 }'
}

StateReds() {

  printf '\n## Reds\n\nWhat is declared to fail. A standing red is a finding, never a licence.\n\n'
  printf '%s' "$EXPECT_FAIL" | tr ' ' '\n' | grep . |
    awk -F: '{ n += ($2 == "" ? 1 : $2); group = $1; sub("/[^/]*$", "", group); g[group]++ }
             END { printf "**%d case(s) declared red.**\n\n| cases | family |\n|---|---|\n", n
                   for (k in g) printf "| %d | `%s` |\n", g[k], k }'
}

StateReach() {

  printf '\n## Reach\n\nSuites reaching past `include/` into `src/`, which CLAUDE.md forbids.\n\n'
  sed -n '/^LayerIncludes()/,/^}/p' "$0" | grep -E "^ +[a-z].*\) printf" |
    awk -F') printf' '{ names = $1; reaches = ($2 ~ /-Isrc/)
                        gsub(/^[ \t]+|[ \t]+$/, "", names)
                        many = split(names, each, /[ \t]*\|[ \t]*/)
                        for (at = 1; at <= many; ++at) {
                          if (reaches) { past++ } else { alone = alone "- `" each[at] "`\n" }
                          all++ } }
                 END { printf "**%d of %d** declared suite(s) are granted a `-Isrc` path (board:1879).\n\nReaching the library through `include/` alone:\n\n%s", past, all, alone }'
}

# WHAT AN ORACLE ACTUALLY JUDGES, and it is not the case count. `test262: 813` reads as "the
# language stands" and the 813 are the EXPRESSION chapters -- I read that number as a capability
# and built an item on it (board:1982). The upstream path in each manifest's `statedAt` is the
# vendor's own chapter, so the reach here is theirs and not our naming, and the common prefix says
# in one line how narrow or wide the slice is.
# A CLIENT'S LINE COUNT IS A MEASUREMENT OF THE DOOR, which is why it is on this page and not in
# Mass beside `src/`. The two counts mean opposite things: a heavy source under `src/` is a
# complexity finding about that file, while a heavy client is a finding about the INTERFACE -- the
# client had to write what the door would not give it. Neither benchmark ships a four-line client
# (Unreal's samples are content plus a launcher, RAGE's game IS its client), so the number has no
# outside reference and only its own direction: it must fall.
StateClients() {
  printf '\n## Clients\n\nWhat a product costs to write on this engine. A client that needs much code is a\nfinding about the DOOR, never about the client.\n\n| lines | units | client | reaches |\n|---|---|---|---|\n'
  for clientDir in $(find apps -mindepth 1 -maxdepth 1 -type d | sort); do
    clientUnits=$(find "$clientDir" -name '*.cpp' -o -name '*.h' | sort)
    [ -n "$clientUnits" ] || continue
    clientLines=$(cat $clientUnits | wc -l | tr -d ' ')
    clientCount=$(printf '%s\n' "$clientUnits" | wc -l | tr -d ' ')
    # THE HONEST COLUMN IS WHETHER IT LINKS, not whether it compiles. An include path into `src/`
    # is one way to reach past the door and the LINKER finds the other: a client that needs a
    # symbol `liboutshine.a` does not export is reaching past it just as surely, and this tree
    # declares one such client in EXPECT_UNLINKED. Reporting only the include paths would print
    # "include/ alone" over a standing red.
    clientReach=$(LayerIncludes "${clientDir#apps/}/src" 2>/dev/null | tr ' ' '\n' | grep -c '^-Isrc' || true)
    clientStanding='`include/` alone'
    [ "${clientReach:-0}" = 0 ] || clientStanding="$clientReach path(s) into \`src/\`"
    for clientRed in $EXPECT_UNLINKED; do
      case "$clientRed" in "$clientDir"/*) clientStanding="**does not link from the library alone**" ;; esac
    done
    printf '| %s | %s | `%s` | %s |\n' "$clientLines" "$clientCount" "$clientDir" "$clientStanding"
  done
}

StateCorpora() {
  printf '\n## Corpora\n\nWhat each outside oracle judges, from the upstream path every case manifest records.\nThe count is cases; the reach is chapters. A wide count over a narrow reach is still narrow.\n\n| corpus | cases | all under | chapters |\n|---|---|---|---|\n'
  find test -name manifest.json -not -path 'test/harness/*' -print0 |
    xargs -0 awk '
      FNR == 1 { split(FILENAME, seg, "/"); corpus = seg[2] "/" seg[3]; taken = 0; here[corpus] = 1 }
      taken { next }
      /"statedAt"/ {
        line = $0
        # A CORPUS WITHOUT A CHAPTER PATH IS STILL A CORPUS. GeographicLib states its criterion
        # against a DOCUMENT rather than a commit, so no path can be derived -- and dropping it
        # for that would make this page silent about an oracle the tree runs.
        if (!match(line, /\/[0-9a-f][0-9a-f]+\//)) { cases[corpus]++; taken = 1; next }
        rest = substr(line, RSTART + RLENGTH)
        sub(/".*$/, "", rest)
        sub(/\/[^\/]*$/, "", rest)
        if (rest == "") { next }
        holds[corpus "\t" rest]++
        cases[corpus]++
        taken = 1
      }
      END {
        for (k in holds) { printf "%s\t%d\n", k, holds[k] }
        for (c in cases) { printf "TOTAL\t%s\t%d\n", c, cases[c] }
      }' > "$BUILD/log/corpus-reach"
  sort -t"$tab" -k1,1 -k3,3rn "$BUILD/log/corpus-reach" | awk -F'\t' '
    $1 == "TOTAL" { total[$2] = $3; next }
    {
      chapters[$1] = chapters[$1] ", " $2 " (" $3 ")"
      if (!($1 in shared)) { shared[$1] = $2 }
      else {
        was = split(shared[$1], older, "/"); now = split($2, newer, "/")
        keep = ""
        for (i = 1; i <= was && i <= now && older[i] == newer[i]; i++) {
          keep = (keep == "") ? older[i] : keep "/" older[i]
        }
        shared[$1] = keep
      }
      count[$1]++
    }
    END {
      for (c in total) {
        line = substr(chapters[c], 3)
        if (length(line) > 130) { line = substr(line, 1, 127) "..." }
        if (count[c] == 0) {
          printf "| `%s` | %d | *stated against a document, no chapter path* | -- |\n", c, total[c]
          continue
        }
        printf "| `%s` | %d | `%s` | %d -- %s |\n", c, total[c],
               (shared[c] == "" ? "nothing shared" : shared[c] "/"), count[c], line
      }
    }' | sort
}

StateDecided() {

  printf '\n## Decided\n\nNamed constants standing as a bare literal, whose origin is elsewhere.\n\n| constants | file |\n|---|---|\n'
  grep -rnE '(constexpr|const) +(double|float|int|size_t|unsigned|uint[0-9]+_t|long) +k[A-Z][A-Za-z0-9_]* *= *-?[0-9]' \
    src --include=*.h --include=*.cpp 2>/dev/null | grep -v '^src/assets/' |
    sed -E 's|^([^:]*):[0-9]*:.*(k[A-Z][A-Za-z0-9_]*) *= *([^;,]*).*|\1 \2 \3|' |
    grep -vE ' -?(0|1|2|4|8|16|32|64|128|256|512|1024|2048|4096)(\.0)?[fu]? *$' |
    grep -vE ' -?(0\.0|1\.0|0\.5)f? *$' | grep -vE ' 0x' |
    grep -vE ' [^ ]*[A-Za-z_][A-Za-z0-9_]*$' |
    awk '{ n[$1]++ } END { for (f in n) printf "%5d %s\n", n[f], f }' | sort -rn | head -10 |
    awk '{ printf "| %s | `%s` |\n", $1, $2 }'
}

StateProgress() {

  # PROGRESS IS COUNTED FROM THE BOARD, because the board is where the target already lives and a
  # second list would be a second spelling of it. An item declares `Progress: <area>` and its
  # checkboxes are that area's predicates. A ticked box must NAME ITS PROOF; one that does not,
  # or whose proof is not in the tree, is reported as an unproven tick and does NOT count as held.
  #
  # Predicates state a BEHAVIOUR or a REACHABILITY, never a name. Counting declared class names
  # would have scored the world generators at 100 per cent while 6528 lines of them sat in the
  # archive with no path from any declaration -- existence is not the question.
  #
  # The denominator is visible and it GROWS: filing a predicate lowers the percentage, because
  # discovering work is not progress.
  #
  # A proof is a CASE (`harness/...`, checked to be a file or a suite directory) or an AUDIT flag
  # (`--audit-layers`, checked to be an option this runner parses). Both run in the gate and both
  # refuse; an audit is not a lesser proof for having no `.cpp`.
  progressTally=$BUILD/log/progress
  progressBare=$BUILD/log/progress-unproven
  tab=$(printf '\t')
  : > "$progressBare"
  : > "$progressTally"
  for item in board/*.md; do
    [ -e "$item" ] || continue
    slug=$(sed -n 's|^Progress:[ ]*||p' "$item" | head -1)
    [ -n "$slug" ] || continue
    ticket=$(basename "$item")
    awk -F'\t' '
      function flush() {
        if (state == "x") { printf "x\t%s\t%s\n", (proof == "" ? "-" : proof), said }
        else if (state == " ") { printf "o\t-\t%s\n", said }
        state = ""
      }
      /^- \[.\]/ { flush(); state = substr($0, 4, 1); proof = ""; said = substr($0, 7); next }
      /proof:/ { if (state != "") { line = $0; sub(/^.*proof:[ \t]*/, "", line); proof = line } }
      END { flush() }
    ' "$item" |
      while IFS="$tab" read -r kind named said; do
        if [ "$kind" = o ]; then
          printf '%s open %s\n' "$slug" "$ticket"
          continue
        fi
        # A TICK MAY NAME MORE THAN ONE PROOF and every one of them must stand: a predicate held
        # by two cases is held by neither if one is missing, so the comma is a conjunction.
        # A PROOF LINE NAMES ITS CASES AND MAY CARRY WHAT THEY READ (board:2003). A token is a
        # proof CLAIM when it starts with one of test/'s six suite roots or with --audit; every
        # other word is the reading and is not a path. `33/33` is prose; `outshine/door` is a
        # claim. A tick that makes no claim names no proof.
        holds=no
        for oneProof in $(printf '%s' "$named" | tr ',' ' '); do
          case "$oneProof" in
            geographiclib/*|harness/*|khronos/*|outshine/*|test262/*|wpt/*) ;;
            --audit*) grep -q -- "$oneProof)" test/run.sh && { holds=yes; continue; }
                      holds=no; break ;;
            *) continue ;;
          esac
          oneProof=${oneProof%%[!A-Za-z0-9_/.-]*}
          if [ -f "test/$oneProof.cpp" ] || [ -d "test/$oneProof" ]; then
            holds=yes
            continue
          fi
          holds=no
          break
        done
        if [ "$holds" = yes ]; then
          printf '%s held %s\n' "$slug" "$ticket"
        else
          printf '%s unproven %s\n' "$slug" "$ticket"
          printf -- '- `%s` in [%s](board/%s) names `%s` -- %s\n' \
            "$slug" "${ticket%%_*}" "$ticket" "$named" "$(printf '%s' "$said" | cut -c1-96)" \
            >> "$progressBare"
        fi
      done >> "$progressTally"
  done

  printf '\n## Progress\n\nNine areas against RAGE and Unreal, counted from `board/` where the target already\nlives. A ticked predicate must NAME ITS PROOF -- a case or an audit flag -- and a tick\nwhose proof this tree does not hold is reported rather than counted.\n\n| area | predicates held | share | items | note |\n|---|---|---|---|---|\n'
  if [ -s "$progressTally" ]; then
    sort "$progressTally" | awk '
      { n[$1 " " $2]++; areas[$1] = 1
        if ($2 != "held" && index(waits[$1], $3) == 0) { waits[$1] = waits[$1] " " $3 } }
      END {
        for (a in areas) { order[++k] = a }
        for (i = 1; i < k; i++) {
          for (j = i + 1; j <= k; j++) {
            if (order[j] < order[i]) { s = order[i]; order[i] = order[j]; order[j] = s }
          }
        }
        for (i = 1; i <= k; i++) {
          a = order[i]
          held = n[a " held"] + 0; waiting_n = n[a " open"] + 0; bare = n[a " unproven"] + 0
          total = held + waiting_n + bare
          if (total == 0) { continue }
          printf "| `%s` | %d/%d | %d%% | ", a, held, total, int(100.0 * held / total + 0.5)
          split(waits[a], waiting, " ")
          first = 1
          for (w = 1; w in waiting; w++) {
            if (waiting[w] == "") { continue }
            number = waiting[w]; sub(/_.*$/, "", number)
            printf "%s[%s](board/%s)", (first ? "" : ", "), number, waiting[w]
            first = 0
          }
          printf " |"
          if (bare > 0) { printf " %d tick(s) name no proof |", bare }
          else { printf " |" }
          printf "\n"
        }
      }'
  else
    printf 'No item declares a `Progress:` area yet -- the table is EMPTY, not complete.\n'
  fi

  # A COUNT OF UNPROVEN TICKS IS NOT A REPORT. Saying "1 tick names no proof" and withholding
  # WHICH one makes a reader walk the board by hand to find it, which is the work this page exists
  # to have already done.
  if [ -s "$progressBare" ]; then
    printf '\nTicked, but the named proof is not in this tree:\n\n'
    sort -u "$progressBare"
  fi
}

if [ "$STATE" = 1 ]; then
  printf '# outshine\n\n'
  printf 'What the tree **is**, generated by `make` and written by no hand. TARGET lives in\n'
  printf '`CLAUDE.md`; where the two disagree, this page is the tree.\n'
  StateProgress
  StateDoor
  StateShape
  StateTiers
  StateMass
  StateCarpet
  StateTwins
  StateStranded
  StateAccess
  StateReds
  StateReach
  StateClients
  StateCorpora
  StateDecided
  exit 0
fi

# EVERY NUMBER CARRIES ITS ORIGIN. A named constant whose value is not 0, 1, a power of two or a
# half is a DECISION, and a decision that is only a literal has its reason nowhere -- CLAUDE.md
# asks for derived, measured or [SET] with a unit and a population, and the place that carries it
# is the constant's board item and the commit that set it. This walk cannot read a reason, so it
# counts the decisions and refuses when the count moves: what is there is declared, and nothing
# new joins it silently.
# PRIVATE IS THE DEFAULT AND A WIDER DOOR JUSTIFIES ITSELF. C++ offers three levels and only one
# of them costs nothing: what is private can be changed. `protected:` exists for inheritance
# alone, in a tree whose rule is composition over it, and a DATA member left public in a class is
# an invariant nobody can hold. Neither is forbidden -- the provider hierarchy earns its keep --
# so this walk counts them, and the gate refuses the day either count moves.
if [ "$AUDIT_ACCESS" = 1 ]; then
  shielded=$(grep -rc '^ *protected:' src --include=*.h 2>/dev/null |
    awk -F: '{ n += $2 } END { print n + 0 }')
  bare=$(awk '
    function opens(s,  n) { n = gsub(/\{/, "{", s); return n }
    function closes(s,  n) { n = gsub(/\}/, "}", s); return n }
    { line = $0 }
    inclass == 0 && line ~ /^[ \t]*class[ \t]+[A-Za-z_]/ && line !~ /;[ \t]*$/ {
      inclass = 1; access = "private"; depth = opens(line) - closes(line); next
    }
    inclass == 0 { next }
    {
      before = depth
      depth += opens(line) - closes(line)
      if (depth <= 0) { inclass = 0; next }
      if (before != 1) { next }
      if (line ~ /^[ \t]*public:/) { access = "public"; next }
      if (line ~ /^[ \t]*(private|protected):/) { access = "private"; next }
      if (access != "public") { next }
      if (line ~ /\(/ || line ~ /\)/ || line ~ /using/ || line ~ /return/) { next }
      if (line ~ /^[ \t]*(template|friend|typedef)/) { next }
      if (line ~ /^[ \t]*(class|struct|enum|union)[ \t]/) { next }
      if (line ~ /^[ \t]+[A-Za-z_][A-Za-z_0-9:<>,& *]*[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]*(=|\[|;)/) { n++ }
    }
    END { print n + 0 }' $(find src -name '*.h' -not -path 'src/assets/*' | sort))
  printf 'AUDIT %s protected section(s), %s public data member(s) in a class\n' "$shielded" "$bare"
  if [ "$shielded" != "$PROTECTED" ] || [ "$bare" != "$OPENDATA" ]; then
    grep -rn '^ *protected:' src --include=*.h 2>/dev/null
    printf 'AUDIT the declaration says %s protected and %s public data -- private is the default\n' \
      "$PROTECTED" "$OPENDATA"
    printf 'AUDIT and a wider door justifies itself in the item that widened it\n'
    exit 1
  fi
  printf 'AUDIT access as declared: what is wider than private is counted and no wider\n'
  exit 0
fi

if [ "$AUDIT_NUMBERS" = 1 ]; then
  decided=$BUILD/log/decided-numbers
  mkdir -p "$BUILD/log"
  grep -rnE '(constexpr|const) +(double|float|int|size_t|unsigned|uint[0-9]+_t|long) +k[A-Z][A-Za-z0-9_]* *= *-?[0-9]' \
    src --include=*.h --include=*.cpp 2>/dev/null |
    grep -v '^src/assets/' |
    sed -E 's|^([^:]*:[0-9]*):.*(k[A-Z][A-Za-z0-9_]*) *= *([^;,]*).*|\1 \2 = \3|' |
    grep -vE '= *-?(0|1|2|4|8|16|32|64|128|256|512|1024|2048|4096)(\.0)?[fu]? *$' |
    grep -vE '= *-?(0\.0|1\.0|0\.5)f? *$' |
    grep -vE '= *0x' |
    grep -vE '= *[^;]*[A-Za-z_][A-Za-z0-9_]*' | sort > "$decided"
  many=$(wc -l < "$decided" | tr -d ' ')
  if [ "$many" != "$DECIDED" ]; then
    cat "$decided"
    printf 'AUDIT %s decision(s) stand as a bare literal and the declaration says %s -- every\n' \
      "$many" "$DECIDED"
    printf 'AUDIT number carries its origin, and a walk cannot read a reason, so it counts them\n'
    exit 1
  fi
  printf 'AUDIT %s decision(s) stand as a bare literal, as declared\n' "$many"
  exit 0
fi

if [ "$AUDIT_LAYERS" = 1 ]; then
  mkdir -p "$BUILD/log"

  # THE TWIN CHECK COMES FIRST because the judgement below resolves an include by its BASENAME:
  # with two headers sharing one name there is no fact to judge, and a crossing between them would
  # go unseen. So this refuses before it decides, rather than deciding on an ambiguity.
  twinned=$(find src include -name '*.h' | sed 's|.*/||' | sort | uniq -d)
  if [ -n "$twinned" ]; then
    printf 'AUDIT two headers share one basename, and this walk resolves an include by basename\n'
    twinHeaders=$(find src include -name '*.h' | sort)
    printf '%s\n' "$twinned" | while IFS= read -r name; do
      printf 'AUDIT   %-24s %s\n' "$name" \
        "$(printf '%s\n' "$twinHeaders" | grep "/$name\$" | tr '\n' ' ')"
    done
    printf 'AUDIT so a tier crossing between them would go unseen -- one name, one header\n'
    exit 1
  fi

  owners=$BUILD/log/header-tiers
  find src -name '*.h' -not -path 'src/assets/*' |
    sed -e 's|^src/\([^/]*\)/.*/\([^/]*\)$|\2 \1|' -e 's|^src/\([^/]*\)/\([^/]*\)$|\2 \1|' |
    sort -u > "$owners"
  byModule=$BUILD/log/module-of-header
  find src -name '*.h' -not -path 'src/assets/*' |
    sed -e 's|^src/\([^/]*/[^/]*\)/.*/\([^/]*\)$|\2 \1|' \
        -e 's|^src/\([^/]*/[^/]*\)/\([^/]*\.h\)$|\2 \1|' \
        -e 's|^src/\([^/]*\)/\([^/]*\.h\)$|\2 \1|' | sort -u > "$byModule"

  # EVERY TIER DECLARES, AND A DIRECTORY THAT DOES NOT IS A REFUSAL. The reach now lives beside
  # the module in `src/<tier>/reaches`, so a missing file is a new directory nobody declared --
  # and skipping it would audit everything EXCEPT the thing just added, which is the exact moment
  # a layering breaks. Silence and "reaches nothing" are different claims; the second is one line
  # in a file.
  reachTable=$BUILD/log/tier-reaches
  : > "$reachTable"
  undeclaredTier=0
  for tier in $(find src -mindepth 1 -maxdepth 1 -type d | sed 's|^src/||' | sort); do
    case "$tier" in assets) continue ;; esac
    if ! tierReaches=$(LayerReaches "$tier"); then
      printf 'AUDIT src/%s declares no reach -- write src/%s/reaches, empty if it reaches nothing\n' \
        "$tier" "$tier"
      undeclaredTier=1
      continue
    fi
    printf '%s %s\n' "$tier" "$tierReaches" >> "$reachTable"
  done
  if [ "$undeclaredTier" = 1 ]; then exit 1; fi

  # BOTH JUDGEMENTS READ THE SAME LINE ONCE. The tier crossing and the module cycle are two
  # questions about the same include, and asking them in two walks spent an `awk` per include line
  # twice over -- seven seconds for a fact that is one pass over the tree.
  awk -v ownerFile="$owners" -v moduleFile="$byModule" -v reachFile="$reachTable" '
    BEGIN {
      while ((getline line < ownerFile) > 0) { split(line, field, " "); tierOf[field[1]] = field[2] }
      while ((getline line < moduleFile) > 0) { split(line, field, " "); moduleOf[field[1]] = field[2] }
      while ((getline line < reachFile) > 0) {
        width = split(line, field, " ")
        audited[field[1]] = 1
        for (i = 2; i <= width; i++) { reaches[field[1] " " field[i]] = 1 }
      }
    }
    FNR == 1 {
      within = FILENAME
      sub(/^src\//, "", within)
      depth = split(within, part, "/")
      tier = part[1]
      from = (depth >= 3) ? part[1] "/" part[2] : (depth == 2 ? part[1] : within)
      delete already
    }
    /^#include "/ {
      head = $0
      sub(/^#include "/, "", head); sub(/".*$/, "", head); sub(/^.*\//, "", head)
      if (head in already) { next }
      already[head] = 1
      held = tierOf[head]
      if (held != "" && (tier in audited) && held != tier && !((tier " " held) in reaches)) {
        printf "AUDIT %s is in the %s tier and includes %s from %s, which %s does not reach\n",
               FILENAME, tier, head, held, tier
        crossed++
      }
      to = moduleOf[head]
      if (to != "" && to != from) { pair[from "|" to] = 1 }
    }
    END {
      for (edge in pair) {
        split(edge, side, "|")
        if (((side[2] "|" side[1]) in pair) && side[1] < side[2]) {
          printf "AUDIT %s and %s include each other -- a cycle is one module spelled in two directories\n",
                 side[1], side[2]
          crossed++
        }
      }
      if (crossed == 0) {
        printf "AUDIT layered: every source includes only what its tier declares it reaches, and no module includes the module that includes it\n"
        exit 0
      }
      printf "AUDIT %d include(s) cross the declared layering\n", crossed
      exit 1
    }
  ' $(find src \( -name '*.cpp' -o -name '*.h' \) -not -path 'src/assets/*' | sort)
  exit $?
fi

if [ "$AUDIT" = 1 ]; then
  bad=0
  for suiteDir in $(find test apps -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u); do
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
  for suiteDir in $(find test apps -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u); do
    groups=$(LayerGroups "$suiteDir" 2>/dev/null) || continue
    for group in $groups; do
      if [ -d "$group" ]; then find "$group" -maxdepth 1 -name '*.cpp'; else printf '%s\n' "$group"; fi
    done
  done | sort -u > "$BUILD/audit-linked.txt"
  find src -name '*.cpp' -not -path 'src/assets/*' | sort > "$BUILD/audit-carried.txt"
  stranded=$(comm -23 "$BUILD/audit-carried.txt" "$BUILD/audit-linked.txt")
  strandedCount=$(printf '%s' "$stranded" | grep -c . || true)
  if [ "$strandedCount" != "$STRANDED" ]; then
    printf '%s\n' "$stranded" | sed 's|^|AUDIT no suite links |'
    printf 'AUDIT %s source(s) reach no suite, and the declaration says %s -- a source no suite lists never LINKS, so nothing it holds is ever proven\n' \
      "$strandedCount" "$STRANDED"
    bad=1
  else
    printf 'AUDIT %s source(s) reach no suite, as declared\n' "$strandedCount"
  fi
  archived=0
  missing=0
  for source in $(find src -name '*.cpp' -not -path 'src/assets/*' | sort); do
    layer=$(dirname "$source")
    if GroupIncludes "$layer" >/dev/null 2>&1 || GroupIncludes "$source" >/dev/null 2>&1; then
      archived=$((archived + 1))
    else
      printf 'AUDIT no library group builds %s\n' "$source"
      missing=$((missing + 1))
      bad=1
    fi
  done
  printf 'AUDIT %s source(s) reach the archive, %s do not\n' "$archived" "$missing"
  [ "$bad" = 0 ] && printf 'AUDIT clean: every suite lists each source once, every source reaches the archive\n'
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
    auditSuites=$(find test apps -name '*.cpp' | sed 's|/[^/]*\.cpp$||' | sed 's|^test/||' | sort -u)
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
BuildGroup src/base/format/Json.cpp || Die "the prune's reader did not build"
pruneStale=no
[ -x "$BUILD/prune" ] || pruneStale=yes
[ "test/harness/shared/Prune.cpp" -nt "$BUILD/prune" ] && pruneStale=yes
for pruneObject in $OBJECTS; do
  [ "$pruneObject" -nt "$BUILD/prune" ] && pruneStale=yes
done
if [ "$pruneStale" = yes ]; then
  $CXX test/harness/shared/Prune.cpp $OBJECTS $CXXSTD $OPT $WARN -Itest/harness/shared -Itest/harness/shared/render -Isrc/base/math -Isrc/base/geo -Isrc/base/format -Isrc/base/spatial -Isrc/content/shade -Isrc/world/weather -Isrc/world/sky -Isrc/base/io -Isrc/content/gltf -o "$BUILD/prune" ||
    Die "the prune did not build"
fi
PRUNE_MARKER=$BUILD/prune.marker


TREES=test
[ -z "$SUITES" ] && TREES="test apps"
for named in $SUITES; do
  case "$named" in
    apps | apps/*) TREES="test apps" ;;
  esac
done

TESTS=""
PROGRAMS=""
for candidate in $(find $TREES -name '*.cpp' | sort); do
  candidateLayer=$(dirname "${candidate#test/}")
  if Programs "$candidateLayer" >/dev/null; then
    PROGRAMS="$PROGRAMS $candidate"
    continue
  fi
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
# hostile-parser layers (board:1743), warm, WITH the hourly nudge's own worktree gate
# building beside it -- the concurrent-nest case this tree mandates every hour, not an idle
# machine. Measured: 100.1 / 104.1 / 153.9 s of run, worst 153.9 -> the bound is that worst
# measurement times 1.5 for the machine's own weather = 230000 ms. A cold rebuild does not
# reach it either, because builds stand beside it.
# board:1876: test/unit IS the regression gate and runs after every change, so it must be fast
# and it must be the whole of what runs by default. harness/claims and test/render are targeted
# AUDITS -- they answer a question somebody asked, and they run when named.
# board:1876: test/unit is gone -- it asserted the shape of a moving architecture. What runs by
# default is what is INVARIANT: the established corpora, whose truth does not depend on our
# design. tools and apps run by name; apps/driver is the integration test and the architect's
# subject.
NAMED_ONLY="apps"
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
# A case directory holds an INPUT when something other than its own bookkeeping is in it.
# board:1839's .prepared-by is bookkeeping and so is the manifest: counting either as an input
# made every pruned case look filled, and the rebuild path never fired for one.
HoldsInput() {
  [ -n "$(ls -A "$1" 2>/dev/null | grep -v -e '^manifest.json$' -e '^\.prepared-by$')" ]
}

RebuildOwner() {
  ownerMap=$BUILD/log/manifest-owners
  if [ "$OWNER_MAP_BUILT" != yes ]; then
    OWNER_MAP_BUILT=yes
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
    HoldsInput "$PREPARED/$ownerDir" && continue
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
  HoldsInput "$2" && return 0
  printf 'run.sh: %s WAS prepared and its input is gone -- it carries a manifest, so this is a corpus removed under a reader and not a fetch that never happened; rebuilding (board:1789, 1797)\n' \
    "${1#test/}" >&2
  before=$(Now)
  if python3 test/harness/shared/corpus/prepare.py all --manifest "$1/manifest.json" \
       > "$BUILD/log/$(printf '%s' "${1#test/}" | tr / -)-rebuild.log" 2>&1; then
    printf 'run.sh: rebuilt %s in %s ms\n' "${1#test/}" "$(( $(Now) - before ))" >&2
    mkdir -p "$2" && printf '%s' "$NEST" > "$2/.prepared-by"
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
declaredUnprepared=0
staleUnprepared=0
partialCases=0
PARTIAL_SAID=""
undeclaredSkips=0
inverted=0

peakKib=0
endKib=0
prunedCases=0
notMine=0
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

# board:1839: a run that prunes nothing still has a peak, and it is the size the corpus already
# stood at -- sampled once so the number is never a zero that reads like an empty disk
SampleSuite

# the nest carries a per-checkout identity and the corpus does not, so a second runner in a
# second checkout shares the 26 GB -- and pruning it is a DELETE. Sharing the bytes is worth
# keeping; sharing the right to remove them is not, so a runner that does not hold the
# corpus claim reads and never prunes (board:1789).
PruneCase() {
  [ "$CORPUSLOCK_MINE" = yes ] || return 0
  pruneCase=$1
  prunePrepared=$2
  # the corpus is SHARED between checkouts and worth sharing at 26 GB; the right to delete from
  # it is not. A case carries the nest that prepared it, and only that nest prunes it, so a
  # second runner reading the same directory cannot lose a subject mid-run (board:1789)
  prunePreparer=$prunePrepared/.prepared-by
  if [ "$(cat "$prunePreparer" 2>/dev/null)" != "$NEST" ]; then
    notMine=$((notMine + 1))
    return 0
  fi
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
    PASS)
      passed=$((passed + 1))
      if DeclaredUnprepared "$recordId"; then
        staleUnprepared=$((staleUnprepared + 1))
        printf 'run.sh: %s PREPARED and EXPECT_UNPREPARED still names it -- the declaration outlived what it declared, so take it out\n' \
          "$recordId" >&2
      fi
      ;;
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
      if DeclaredUnprepared "$recordId"; then
        declaredUnprepared=$((declaredUnprepared + 1))
        printf 'run.sh: %s has no oracle and EXPECT_UNPREPARED says why -- board:1923\n' \
          "$recordId" >&2
      else
        unprepared=$((unprepared + 1))
        printf 'run.sh: %s has no prepared input and its rebuild did not give it one -- %s\n' \
          "$recordId" "$BUILD/log/$(printf '%s' "$recordId" | tr / -)-rebuild.log" >&2
      fi
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
printf '%s tests: %s PASS  %s FAIL  %s TIMEOUT  %s SIGNAL  %s BUILD  %s SKIP  %s UNPREPARED  %s PARTIAL  in %s ms\n' \
  "$total" "$passed" "$failed" "$timedout" "$signalled" "$unbuilt" "$skipped" "$unprepared" \
  "$partialCases" "$elapsedMs"
if [ "$FAST_GATE" = yes ]; then
  EverySourceStillCompiles || compileBlind=1
  EveryProgramStillLinks || compileBlind=1
  WhatNoCorpusJudges
fi
# board:1810: a case that judged part of its subject may not read as one that judged all of it.
# The share is in the case's own PARTIAL line and the truncation point moves with the machine,
# so the trailer carries both the case and the share it reached.
for said in $PARTIAL_SAID; do
  partialLog=$BUILD/log/$(printf '%s' "$said" | tr / -).log
  partialShare=$(sed -n 's/^PARTIAL \([0-9.]*\) .*/\1/p' "$partialLog" | head -1)
  partialOf=$(sed -n 's/^PARTIAL [0-9.]* //p' "$partialLog" | head -1)
  # board:1845: a share of ZERO is not "part of its subject", it is none of it -- and the two
  # read differently to anyone counting on the trailer
  partialWord=PART
  case "$partialShare" in 0|0.0|0.00*) partialWord=NONE ;; esac
  printf 'run.sh: %s JUDGED %s OF ITS SUBJECT -- %s of %s, so this trailer says nothing about the rest and a run on another machine will stop somewhere else (board:1810, 1845)\n' \
    "$said" "$partialWord" "$partialShare" "$partialOf" >&2
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

# board:1839: the disk number is a bound this tree quotes at 26 GB and measured nowhere, so it
# is published in EVERY run and not only in runs that deleted something
[ "$endKib" -gt 0 ] && printf \
  'test corpora: peak %s MB, %s MB at the end\n' "$((peakKib / 1024))" "$((endKib / 1024))"
[ "$prunedCases" -gt 0 ] && printf \
  'test corpora: %s cases pruned, %s files and %s MB declined, %s file(s) left standing (each case: %s/*-prune.log)\n' \
  "$prunedCases" "$prunedFiles" "$((prunedKib / 1024))" "$stayedFiles" "$BUILD/log"
[ "$notMine" -gt 0 ] && printf \
  'test corpora: %s case(s) left untouched because this nest did not prepare them -- the corpus is shared and the right to delete from it is not (board:1789)\n' \
  "$notMine"

# the bound holds the RUN: compiling is the nest's business, and a cold rebuild after a
# header edit is not a slow test -- the measured span excludes the build phases, which are
# printed beside it so a bloating build is still visible (board:1735)
gateRunMs=$(( elapsedMs - builtSpentMs ))
if [ "$FAST_GATE" = yes ] && [ "$gateRunMs" -le "$kFastGateBoundMs" ]; then
  printf 'run.sh: gate headroom %s ms of %s (run %s ms, builds %s ms beside the bound)\n' \
    "$((kFastGateBoundMs - gateRunMs))" "$kFastGateBoundMs" "$gateRunMs" "$builtSpentMs"
fi
red=$((failed + timedout + signalled + unbuilt + undeclaredSkips + unprepared + compileBlind + staleUnprepared))

overran=0
if [ "$FAST_GATE" = yes ] && [ "$gateRunMs" -gt "$kFastGateBoundMs" ]; then
  overran=1
  printf 'run.sh: THE FAST GATE OVERRAN ITS BOUND -- %s ms of RUN over the declared %s ms (builds %s ms stood beside the bound): a slow test is a finding, exactly like a slow frame (board:1601, 1735)\n' \
    "$gateRunMs" "$kFastGateBoundMs" "$builtSpentMs" >&2
fi
if [ "$declaredUnprepared" -gt 0 ]; then
  printf 'run.sh: %s arm(s) have no oracle this toolchain can make, declared in EXPECT_UNPREPARED: %s\n' \
    "$declaredUnprepared" "$EXPECT_UNPREPARED" >&2
fi
if [ "$red" -gt 0 ]; then
  printf 'run.sh: %s case(s) are RED and the verdict is theirs, whatever the clock said\n' "$red" >&2
fi

[ "$red" -eq 0 ] && [ "$overran" -eq 0 ]
