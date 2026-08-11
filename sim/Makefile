# fb-sim — the outshine Command Center. Self-contained: this Makefile, Dockerfile, the C++ source
# under src/, and the vendored toolchain under vendor/ (Dawn, emdawnwebgpu, stb, build scripts) all
# live under this one directory. The only cross-directory reference is fb-tiles' public bake-version
# header (../tiles/src/style_ver.h).
#
#   make wasm     build the WASM client -> web/gpu.js + web/gpu.wasm
#   make walk     build the native pedestrian frame oracle (Dawn + fb-tiles terrain) -> build/gpu_walk
#   make world    build the server target: everything EXCEPT render/, no device -> build/fb_world
#   make verify-generators  a generators/ translation unit can name core and NOTHING above it --
#                 the include set is the layering, and a breach is a compile error
#   make verify-clients  both main()s are entry points and nothing else, and the scene is built in
#                 exactly one place -- what one client shows, the other cannot fail to show
#   make image    build the deployable fb-sim web container (podman)
#   make up       run the web host container (serves the WASM app on :8080)
#   make clean    remove build artifacts
SHELL := /bin/bash
.DEFAULT_GOAL := help

# This Makefile's own directory, absolute -- recipes cd here so every path is sim-relative no matter
# whether invoked as `make -C sim wasm`, `cd sim && make`, or from a parent Makefile.
SELF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

EMSDK_DIR ?= $(HOME)/Git/emsdk
EMCC_WARN := -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
CXX_WARN  := -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
CXXSTD    := -std=c++17

# Incremental compiles WITH header dependencies. The object-building recipes below are shell loops, not
# make pattern rules, so make's own prerequisite graph never sees a header: a plain `[ obj -nt src ]`
# guard silently keeps objects built against an OLD header (change State.h and the next `make walk`
# links stale code — and every number measured from that binary is a phantom). DEPFLAGS makes
# each compile write its full prerequisite list to <obj>.d; fb_uptodate re-reads it and rebuilds unless
# the object is newer than its source AND every header it actually included. ($$(...) command
# substitution, not backticks: inside backticks the shell eats one level of backslash even in single
# quotes, which mangles the sed script that strips the .d file's line continuations.)
DEPFLAGS := -MMD -MP

# EVERY THREAD THE CLIENT RUNS, ALLOCATED AT BRING-UP: one for the class grid and the tile pool's
# ceiling (world/TerrainLoader.cpp kMaxTileThreads). A pthread_create beyond the pool has to go back
# to the main thread's event loop to get its Worker, which is a stall on the thread that draws --
# and the pool is created exactly once, while the loading screen is up.
WASM_THREADS := -pthread
WASM_THREADS_LINK := $(WASM_THREADS) -sPTHREAD_POOL_SIZE=7
FB_UPTODATE := fb_uptodate() { o="$$1"; f="$$2"; d="$${o%.o}.d"; if [ ! -f "$$o" ] || [ ! -f "$$d" ] || [ "$$f" -nt "$$o" ]; then return 1; fi; for p in $$(sed -e 's/^[^:]*://' -e 's/\\//g' "$$d"); do if [ ! -e "$$p" ] || [ "$$p" -nt "$$o" ]; then return 1; fi; done; return 0; };

# THE CLIENT'S LINEAR MEMORY, FIXED, and there is now exactly one of it. WebGPU refuses a resizable
# ArrayBuffer as an argument source (setBindGroup and writeBuffer are not [AllowResizable]), so growth
# costs a per-access guard for a growth the graphics API would not survive anyway. The size is the
# module's own measured high-water plus 25 %, rounded up to the next 8 MiB, taken at the tile pool's
# SIX-thread ceiling rather than at whatever this host happens to offer; the measurements and the runs
# they came from are in git log, never here, because a number in a Makefile decays silently.
# ABORTING_MALLOC=0: malloc hands back the null, and core/Heap.cpp is what ends the run — naming the
# item and its bytes, which the toolchain's own abort cannot do.
# STACK_OVERFLOW_CHECK=1 is the same statement for the stack: the sizes below are set from a
# measured peak, and a shadow stack that runs past its end corrupts the heap silently without it.
WASM_MAIN_MEMORY := -sINITIAL_MEMORY=248MB -sSTACK_SIZE=256KB -sDEFAULT_PTHREAD_STACK_SIZE=256KB \
  -sABORTING_MALLOC=0 -sSTACK_OVERFLOW_CHECK=1

DAWN_OUT     := vendor/dawn/out
DAWN_LIBDIR  := $(DAWN_OUT)/src/dawn/native
CURL_COMPAT  := vendor/.compat-headers

# Shared include roots (flat -I per module dir; #include stays bare-filename — which is why moving a
# file between these directories touches no source line). THE INCLUDE SET IS THE LAYERING, and the
# targets are what enforce it: core = shared value types and services; render = Renderer + the WebGPU
# stages; world = World/tile streaming, with world/terrain the lean terrain API under it; clients =
# the entry points. `make world` links without -Isrc/render at all, so an upward include from world/
# is a build failure rather than a rule.
INC_SIM     := -Isrc/core -Isrc/world
INC_MODULES := $(INC_SIM) -Isrc/render
# A GENERATOR'S WHOLE WORLD. Not a subset of the above by accident: `make verify-generators`
# compiles against exactly this and asserts that Renderer and World have no spelling in it.
INC_GENERATORS := -Isrc/core -Isrc/generators
INC_CLIENT := -Isrc/clients
INC_WASM   := $(INC_MODULES) -Isrc/world/terrain
INC_NATIVE := $(INC_MODULES) -Isrc/world/terrain
INC_TILES := -I../tiles/src

# EVERY WebGPU render stage, one class per shader (CLAUDE.md render/stages). A wildcard, not a hand list:
# the wasm and native link lines used to carry two byte-identical copies of it, so adding a stage meant
# remembering both and a browser-only link error was the failure mode. Now a new file in the directory is
# a new stage in BOTH targets, by construction.
RENDER_STAGE_SRCS := $(wildcard src/render/stages/*.cpp)
# ...and the same statement one level up, for Renderer and its neighbours: hand-listing them meant a
# new render/ file linked in one target and not the other.
RENDER_SRCS := $(wildcard src/render/*.cpp) $(RENDER_STAGE_SRCS)

# THE TREE GENERATOR: one mesh out of one species declaration. Linked into
# both clients so that the layer under construction is held to the same -Werror on both toolchains; the
# consumer is the next layer, and `treebench` is what measures it today.
TREE_SRCS := src/world/TreeSpecies.cpp src/world/TreeGrower.cpp src/world/TreeLeaf.cpp \
  src/world/TreeFoliage.cpp src/world/LeafAngleDistribution.cpp

# THE SIMULATION HALF, and it is a LINK statement: nothing in this list may name render/, which is
# what `make world` proves by building it with $(INC_SIM) and no renderer object at all.
SIM_SRCS := src/clients/Sim.cpp src/clients/Scene.cpp src/clients/Mod.cpp \
  src/clients/Animation.cpp src/clients/LogSinks.cpp src/clients/StreamTelemetry.cpp \
  src/core/Log.cpp src/core/Telemetry.cpp src/core/Json.cpp src/core/Keyframes.cpp src/core/CatmullRom.cpp \
  src/core/Heap.cpp src/core/HeapProbe.cpp src/core/StackProbe.cpp \
  src/world/World.cpp \
  src/world/VegetationTemplates.cpp src/world/GroundMaterials.cpp src/world/AlpineLimit.cpp src/world/OsmVector.cpp src/world/OsmField.cpp src/world/ClassField.cpp src/world/ClassStructure.cpp src/world/ClassBuilder.cpp src/world/BuildingField.cpp src/world/WaterField.cpp src/world/TreeField.cpp src/world/Forest.cpp \
  $(TREE_SRCS)

# THE PICTURE HALF over it. Outshine owns the renderer and is the only thing in the tree that builds
# a scene; a client adds an entry point and an output medium over it. Both clients link this list,
# which is what `make verify-clients` holds them to.
APP_SRCS := $(SIM_SRCS) src/clients/Outshine.cpp src/clients/Snapshot.cpp \
  src/clients/SceneRunner.cpp src/clients/SubjectBench.cpp src/clients/Png.cpp \
  src/clients/FileArtifacts.cpp src/clients/HttpPost.cpp src/clients/ServerLog.cpp \
  src/clients/ServerTelemetry.cpp src/clients/FrameTelemetry.cpp \
  src/clients/MemoryTelemetry.cpp \
  src/clients/ServerArtifacts.cpp

# Named, not `build/obj-wasm/*.o`: the glob would link whatever a PREVIOUS target had left in there.
WASM_TERRAIN_OBJS := build/obj-wasm/TerrainLoader.o build/obj-wasm/TilePool.o \
  $(patsubst src/world/terrain/%.cpp,build/obj-wasm/%.o,$(wildcard src/world/terrain/*.cpp))

# THE MODS, whole. A mod is a set of declared scenes and nothing else; the browser gets the same
# directory the native oracle reads off disk, so `<mod> <scene>` means the same two words in both.
MODS_DIR := ../mods

.PHONY: help wasm walk world treebench verify-generators verify-clients image up down restart clean

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

wasm:            ## build the WASM client -> web/gpu.{js,wasm}
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  $(FB_UPTODATE) \
	  mkdir -p build/obj-wasm; \
	  for f in src/world/terrain/*.cpp; do o=build/obj-wasm/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) $(WASM_THREADS) -Isrc/world/terrain -Isrc/core $(INC_TILES) -Ivendor -c -o "$$o"; done; \
	  for f in src/world/TerrainLoader.cpp src/world/TilePool.cpp; do \
	    emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(WASM_THREADS) $(INC_SIM) -Isrc/world/terrain $(INC_TILES) \
	      -c -o build/obj-wasm/$$(basename $$f .cpp).o; done; \
	  emcc $(WASM_TERRAIN_OBJS) \
	    src/clients/AppWasm.cpp src/clients/Walker.cpp $(APP_SRCS) \
	    $(RENDER_SRCS) \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O2 $(EMCC_WARN) -fexceptions \
	    $(INC_WASM) $(INC_CLIENT) -isystem vendor \
	    $(WASM_THREADS_LINK) $(WASM_MAIN_MEMORY) -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_fb_take_http_body \
	    --preload-file web/moon.jpg@/moon.jpg \
	    --preload-file $(MODS_DIR)@/mods \
	    --preload-file assets/world/vegetation.json@/vegetation.json \
	    --preload-file assets/world/species@/assets/world/species \
    --preload-file assets/world/ground-materials.json@/ground-materials.json \
	    -o web/gpu.js; \
	  echo "WASM -> web/gpu.js (mods: $(MODS_DIR))"

walk:            ## build the pedestrian frame oracle (render+world) -> build/gpu_walk
	@cd $(SELF_DIR); set -e; \
	  $(FB_UPTODATE) \
	  PLATFORM_LIBS=""; \
	  if [ "$$(uname -s)" = "Darwin" ]; then \
	    PLATFORM_LIBS="-framework Cocoa -framework IOKit -framework Foundation -framework IOSurface -framework QuartzCore -framework Metal"; \
	  fi; \
	  test -f $(DAWN_LIBDIR)/libwebgpu_dawn.a || bash vendor/build_dawn_native.sh; \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  mkdir -p build/obj-walk; \
	  for f in src/world/terrain/*.cpp; do o=build/obj-walk/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS) -Isrc/world/terrain -Isrc/core $(INC_TILES) -isystem vendor -c -o "$$o"; done; \
	  for f in src/world/TerrainLoader.cpp src/world/TilePool.cpp; do \
	    c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) $(INC_SIM) -Isrc/world/terrain $(INC_TILES) \
	      -isystem $(CURL_COMPAT) -c -o build/obj-walk/$$(basename $$f .cpp).o; done; \
	  c++ src/clients/AppWalk.cpp $(APP_SRCS) \
	    $(RENDER_SRCS) \
	    build/obj-walk/*.o \
	    -std=c++20 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) $(INC_CLIENT) \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor \
	    -isystem $(CURL_COMPAT) \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm $$PLATFORM_LIBS \
	    -o build/gpu_walk; \
	  echo "walk -> build/gpu_walk"

world:           ## build the server target -- everything except render/, no device -> build/fb_world
	@cd $(SELF_DIR); set -e; \
	  $(FB_UPTODATE) \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  mkdir -p build/obj-world; \
	  for f in src/world/terrain/*.cpp; do o=build/obj-world/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS) -Isrc/world/terrain -Isrc/core $(INC_TILES) -isystem vendor -c -o "$$o"; done; \
	  for f in src/world/TerrainLoader.cpp src/world/TilePool.cpp; do \
	    c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) $(INC_SIM) -Isrc/world/terrain $(INC_TILES) \
	      -isystem $(CURL_COMPAT) -c -o build/obj-world/$$(basename $$f .cpp).o; done; \
	  c++ src/clients/WorldMain.cpp $(SIM_SRCS) build/obj-world/*.o \
	    -std=c++20 -O2 $(CXX_WARN) $(INC_SIM) -Isrc/world/terrain $(INC_CLIENT) \
	    -isystem vendor -isystem $(CURL_COMPAT) \
	    -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm \
	    -o build/fb_world; \
	  echo "world -> build/fb_world"

treebench:       ## grow all sixteen species and measure them -> build/treebench (CSV on stdout)
	@cd $(SELF_DIR); set -e; \
	  mkdir -p build; \
	  c++ src/clients/TreeBench.cpp $(TREE_SRCS) src/core/Json.cpp \
	    $(CXXSTD) -O2 $(CXX_WARN) -Isrc/world -Isrc/core \
	    -o build/treebench; \
	  echo "treebench -> build/treebench"

verify-generators: ## a generators/ TU compiles against core and CANNOT name Renderer or World
	@cd $(SELF_DIR); set -e; \
	  c++ src/generators/gate/CoreIsReachable.cpp $(CXXSTD) $(CXX_WARN) $(INC_GENERATORS) -fsyntax-only; \
	  for f in src/generators/gate/RendererIsNotReachable.cpp src/generators/gate/WorldIsNotReachable.cpp; do \
	    if c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -fsyntax-only 2>/dev/null; then \
	      echo "verify-generators: $$f COMPILED -- the include set no longer bounds generators/" >&2; exit 1; \
	    fi; done; \
	  echo "verify-generators: core reachable, render/ and world/ have no spelling in generators/"

# WHAT THE TYPE SYSTEM MUST REFUSE. A rule that only a reviewer enforces is a rule that comes back:
# both of these were live defects before they were compile errors. The negative halves are built with
# the house warning set because -Werror is what turns [[nodiscard]] into a refusal.
verify-types:    ## a value whose misuse must not compile -- and does not
	@cd $(SELF_DIR); set -e; \
	  c++ src/core/gate/GroundSampleIsUsable.cpp $(CXXSTD) $(CXX_WARN) -Isrc/core -fsyntax-only; \
	  for f in src/core/gate/HeightIsNotReachableWithoutItsState.cpp src/core/gate/AnswerIsNotIgnorable.cpp; do \
	    if c++ "$$f" $(CXXSTD) $(CXX_WARN) -Isrc/core -fsyntax-only 2>/dev/null; then \
	      echo "verify-types: $$f COMPILED -- the ground's height is readable without its state" >&2; exit 1; \
	    fi; done; \
	  echo "verify-types: GroundSample usable, its height unreachable without the state"

# ONE PROGRAM, TWO TRANSLATIONS. The gate that would have caught the forest living in one main() for
# ten rounds: an entry point may not build anything, and the scene-building calls exist in exactly
# one translation unit.
verify-clients:  ## check that both entry points are entry points and share one scene builder
	@cd $(SELF_DIR) && python3 tools/verify_clients.py

image:          ## build the deployable fb-sim web container (context = sim/, trimmed by .containerignore)
	cd $(SELF_DIR) && podman build -f Dockerfile -t fb-sim .

up:              ## run the web host container (fb-sim, :8080; sim runs IN the browser)
	@cd $(SELF_DIR) && bash up.sh

down:            ## stop the web host
	-podman rm -f fb-sim 2>/dev/null

restart: down up ## stop and start the web host

clean:           ## remove build artifacts (WASM + walk + world)
	cd $(SELF_DIR) && rm -rf build/obj-wasm build/obj-walk build/obj-world \
	  build/gpu_walk build/fb_world \
	  web/gpu.js web/gpu.wasm web/gpu.data
