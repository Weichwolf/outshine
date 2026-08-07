# fb-sim — the outshine Command Center. Self-contained: this Makefile, Dockerfile, the C++ source
# under src/, and the vendored toolchain under vendor/ (Dawn, emdawnwebgpu, stb, build scripts) all
# live under this one directory. The only cross-directory reference is fb-tiles' public bake-version
# header (../tiles/src/style_ver.h).
#
#   make wasm     build the WASM pedestrian bench + tile worker (depends on worker, so a browser boot
#                 never hangs on a missing worker) -> web/gpu.js + web/fbtileworker.js
#   make worker   build the off-thread tile worker (fetch/decode/mesh/mips) alone -> web/fbtileworker.js
#   make walk     build the native pedestrian frame oracle (Dawn + fb-tiles terrain) -> build/gpu_walk
#   make verify-layers  src/ is a stack and every #include points DOWN it -- the directory tree as a
#                 machine-checked matrix
#   make verify-trees  doc/ (what we want), src/ (what we can) and test/ (what we prove) carry the
#                 same directory tree -- an orphan is a named hole
#   make verify-types  what src/ still knows about concrete aircraft types (rc=1 while > 0)
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
FB_UPTODATE := fb_uptodate() { o="$$1"; f="$$2"; d="$${o%.o}.d"; if [ ! -f "$$o" ] || [ ! -f "$$d" ] || [ "$$f" -nt "$$o" ]; then return 1; fi; for p in $$(sed -e 's/^[^:]*://' -e 's/\\//g' "$$d"); do if [ ! -e "$$p" ] || [ "$$p" -nt "$$o" ]; then return 1; fi; done; return 0; };

DAWN_OUT     := vendor/dawn/out
DAWN_LIBDIR  := $(DAWN_OUT)/src/dawn/native
CURL_COMPAT  := vendor/.compat-headers

# Shared include roots (flat -I per module dir; #include stays bare-filename — which is why moving a
# file between these directories touches no source line). The directories ARE the layers `make
# verify-layers` enforces: core = shared value types and services (State/Mode/Camera/Mat4/Geodesy/
# telemetry/elevation); units = the world-entity identity/pose the renderer draws; render = Renderer +
# the WebGPU stages; world = World/tile streaming, with world/terrain the lean terrain API under it;
# clients = the entry points.
INC_MODULES := -Isrc/core -Isrc/render -Isrc/world -Isrc/units
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

# THE PEDESTRIAN, ONE LIST FOR BOTH CLIENTS: render/ + world/ + exactly the core/ implementations
# those two reference. gpu_walk and the browser link the SAME engine, which is what makes a PNG from
# one a statement about the other.
# THE TREE GENERATOR: one mesh out of one species declaration (doc/render/vegetation.md). Linked into
# both clients so that the layer under construction is held to the same -Werror on both toolchains; the
# consumer is the next layer, and `treebench` is what measures it today.
TREE_SRCS := src/world/TreeSpecies.cpp src/world/TreeGrower.cpp src/world/TreeLeaf.cpp \
  src/world/TreeFoliage.cpp src/world/LeafAngleDistribution.cpp

PEDESTRIAN_SRCS := \
  src/core/Log.cpp src/core/Telemetry.cpp \
  src/world/VegetationTemplates.cpp src/world/GroundMaterials.cpp src/world/OsmVector.cpp src/world/OsmField.cpp src/world/BuildingField.cpp \
  $(TREE_SRCS)

# Named, not `build/obj-wasm/*.o`: the glob would link whatever a PREVIOUS target had left in there.
WASM_TERRAIN_OBJS := build/obj-wasm/TerrainLoader.o \
  $(patsubst src/world/terrain/%.cpp,build/obj-wasm/%.o,$(wildcard src/world/terrain/*.cpp))

# THE ONE SCENE (doc/goal.md §1). Both clients hard-wire it; this is only where the browser build
# finds the file to preload.
SCENE_JSON := ../mods/demo/scene.json

.PHONY: help wasm worker walk treebench verify-layers verify-types verify-trees image up down restart clean

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

wasm: worker     ## build the WASM pedestrian bench + tile worker -> web/gpu.{js,wasm} + web/fbtileworker.{js,wasm}
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  $(FB_UPTODATE) \
	  mkdir -p build/obj-wasm; \
	  for f in src/world/terrain/*.cpp; do o=build/obj-wasm/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) -Isrc/world/terrain -Isrc/core $(INC_TILES) -Ivendor -c -o "$$o"; done; \
	  emcc src/world/TerrainLoader.cpp $(CXXSTD) -O1 $(EMCC_WARN) $(INC_MODULES) -Isrc/world/terrain $(INC_TILES) \
	    -c -o build/obj-wasm/TerrainLoader.o; \
	  emcc $(WASM_TERRAIN_OBJS) \
	    src/clients/AppWasm.cpp src/clients/Scene.cpp src/clients/Snapshot.cpp src/clients/LogSinks.cpp \
	    src/world/World.cpp $(PEDESTRIAN_SRCS) \
	    $(RENDER_SRCS) \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O1 $(EMCC_WARN) -fexceptions \
	    $(INC_WASM) $(INC_CLIENT) \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free \
	    --preload-file web/moon.jpg@/moon.jpg \
	    --preload-file $(SCENE_JSON)@/scene.json \
	    --preload-file assets/world/vegetation.json@/vegetation.json \
    --preload-file assets/world/ground-materials.json@/ground-materials.json \
	    -o web/gpu.js; \
	  echo "WASM -> web/gpu.js + web/fbtileworker.js (scene: $(SCENE_JSON))"

worker:          ## build the off-thread tile worker (fetch/decode/mesh/mips) -> web/fbtileworker.js
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  $(FB_UPTODATE) \
	  	  mkdir -p build/obj-worker; \
	  for f in src/world/terrain/*.cpp; do o=build/obj-worker/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) -Isrc/world/terrain -Isrc/core $(INC_TILES) -Ivendor -c -o "$$o"; done; \
	  emcc src/core/Log.cpp $(CXXSTD) -O2 $(EMCC_WARN) -Isrc/core -c -o build/obj-worker/Log.o; \
	  emcc build/obj-worker/*.o src/clients/TileWorkerMain.cpp src/clients/LogSinks.cpp $(CXXSTD) -O2 $(EMCC_WARN) \
	    -Isrc/render -Isrc/world/terrain -Isrc/core -Isrc/clients $(INC_TILES) \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sEXIT_RUNTIME=0 -sFETCH \
	    -sEXPORTED_FUNCTIONS=_malloc,_free,_fbtw_open,_fbtw_build,_fbtw_dag,_fbtw_verts,_fbtw_nverts,_fbtw_clusters,_fbtw_nclusters,_fbtw_clusterbytes,_fbtw_err,_fbtw_origin,_fbtw_mips,_fbtw_mipbytes,_fbtw_ts,_fbtw_release \
	    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,HEAPF32,HEAPF64 \
	    -o web/fbtileworker.js; \
	  echo "WASM worker -> web/fbtileworker.js"

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
	  c++ src/world/TerrainLoader.cpp $(CXXSTD) -O2 $(CXX_WARN) $(INC_NATIVE) $(INC_TILES) \
	    -isystem $(CURL_COMPAT) -c -o build/obj-walk/TerrainLoader.o; \
	  c++ src/clients/AppWalk.cpp src/clients/Scene.cpp src/clients/Snapshot.cpp src/clients/SubjectBench.cpp \
	    src/clients/LogSinks.cpp \
	    src/world/World.cpp $(PEDESTRIAN_SRCS) \
	    $(RENDER_SRCS) \
	    build/obj-walk/*.o \
	    -std=c++20 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) $(INC_CLIENT) \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor \
	    -isystem $(CURL_COMPAT) \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm $$PLATFORM_LIBS \
	    -o build/gpu_walk; \
	  echo "walk -> build/gpu_walk"

treebench:       ## grow all sixteen species and measure them -> build/treebench (CSV on stdout)
	@cd $(SELF_DIR); set -e; \
	  mkdir -p build; \
	  c++ src/clients/TreeBench.cpp $(TREE_SRCS) src/render/Json.cpp \
	    $(CXXSTD) -O2 $(CXX_WARN) -Isrc/world -Isrc/render -Isrc/core \
	    -o build/treebench; \
	  echo "treebench -> build/treebench"

# The TRIAD gate: doc/ = what we want, src/ = what we can, test/ = what we prove, and the three carry
# the same directory tree. Same reasoning as its two siblings for not being a build prerequisite.
verify-trees:    ## check that doc/, src/ and test/ carry the same directory tree
	@cd $(SELF_DIR) && python3 tools/verify_trees.py

verify-layers:   ## check src/ + test/ against the layer matrix in tools/verify_layers.py
	@cd $(SELF_DIR) && python3 tools/verify_layers.py

# The Prinzip-3 METER, and unlike its siblings it is RED on purpose: it counts what the engine still
# knows about named aircraft types, broken down by how expensive each mention is to remove. Not part
# of the green gate until the count reaches 0 -- tools/verify_types.py says why.
verify-types:    ## count what src/ still knows about concrete aircraft types (rc=1 while > 0)
	@cd $(SELF_DIR) && python3 tools/verify_types.py $(ARGS)

image:          ## build the deployable fb-sim web container (context = sim/, trimmed by .containerignore)
	cd $(SELF_DIR) && podman build -f Dockerfile -t fb-sim .

up:              ## run the web host container (fb-sim, :8080; sim runs IN the browser)
	@cd $(SELF_DIR) && bash up.sh

down:            ## stop the web host
	-podman rm -f fb-sim 2>/dev/null

restart: down up ## stop and start the web host

clean:           ## remove build artifacts (WASM + worker + walk)
	cd $(SELF_DIR) && rm -rf build/obj-wasm build/obj-worker build/obj-walk build/gpu_walk \
	  web/gpu.js web/gpu.wasm web/gpu.data web/fbtileworker.js web/fbtileworker.wasm
