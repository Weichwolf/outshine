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

# EVERY THREAD THE CLIENT RUNS, ALLOCATED AT BRING-UP: one for the class grid, one for the region
# forge, and the tile pool's ceiling (world/TerrainLoader.cpp kMaxTileThreads). A pthread_create
# beyond the pool has to go back
# to the main thread's event loop to get its Worker, which is a stall on the thread that draws --
# and the pool is created exactly once, while the loading screen is up.
WASM_THREADS := -pthread
WASM_THREADS_LINK := $(WASM_THREADS) -sPTHREAD_POOL_SIZE=8
FB_UPTODATE := fb_uptodate() { o="$$1"; f="$$2"; d="$${o%.o}.d"; if [ ! -f "$$o" ] || [ ! -f "$$d" ] || [ "$$f" -nt "$$o" ]; then return 1; fi; for p in $$(sed -e 's/^[^:]*://' -e 's/\\//g' "$$d"); do if [ ! -e "$$p" ] || [ "$$p" -nt "$$o" ]; then return 1; fi; done; return 0; };

# THE CLIENT'S LINEAR MEMORY, FIXED, and there is now exactly one of it. WebGPU refuses a resizable
# ArrayBuffer as an argument source (setBindGroup and writeBuffer are not [AllowResizable]), so growth
# costs a per-access guard for a growth the graphics API would not survive anyway. The size is the
# module's own measured high-water plus 25 %, rounded up to the next 8 MiB, taken at the tile pool's
# SIX-thread ceiling rather than at whatever this host happens to offer; the measurements and the runs
# they came from are in git log, never here, because a number in a Makefile decays silently.
# ABORTING_MALLOC=0: malloc hands back the null, and core/io/Heap.cpp is what ends the run — naming the
# item and its bytes, which the toolchain's own abort cannot do.
# STACK_OVERFLOW_CHECK=1 is the same statement for the stack: the sizes below are set from a
# measured peak, and a shadow stack that runs past its end corrupts the heap silently without it.
WASM_MAIN_MEMORY := -sINITIAL_MEMORY=296MB -sSTACK_SIZE=256KB -sDEFAULT_PTHREAD_STACK_SIZE=256KB \
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
# ONE COMPILE GROUP PER LAYER, and the group is what the layering IS: every source is compiled with
# the include set of its own directory, so an upward include has no spelling rather than a rule. The
# three targets link the same objects -- a set that is wider in one target than in another is a
# breach that builds.
INC_CORE    := -Isrc/core -Isrc/core/io
# A GENERATOR'S WHOLE WORLD. Not a subset of the others by accident: `make verify-generators`
# compiles against exactly this and asserts that Renderer, World and the log have no spelling in it.
# src/core/io is ABSENT on purpose -- Log and TelemetryBus live there, and a generator that cannot
# name them cannot do I/O.
INC_GENERATORS := -Isrc/core -Isrc/generators
# ...and the picture half of the same layer, which the server target never compiles: DrawSource and
# its sink hand clusters and instances to a renderer, and the server has no renderer to hand them to.
INC_DRAW := $(INC_GENERATORS) -Isrc/generators/draw
INC_WORLD  := $(INC_CORE) -Isrc/world -Isrc/world/terrain
INC_RENDER := $(INC_CORE) -Isrc/render -Isrc/render/stages
# THE SIMULATION HALF OF THE CLIENTS, and it is the ONLY place that may name world and generators in
# one translation unit: -Isrc/render is absent, so the server target's half cannot reach a renderer.
INC_SIMHALF := $(INC_CORE) -Isrc/world -Isrc/world/terrain -Isrc/generators -Isrc/clients
# ...and the picture half over it, the one set that holds everything.
INC_CLIENTS := $(INC_SIMHALF) -Isrc/generators/draw $(INC_RENDER)
INC_TILES := -I../tiles/src

# EVERY WebGPU render stage, one class per shader (CLAUDE.md render/stages). A wildcard, not a hand list:
# the wasm and native link lines used to carry two byte-identical copies of it, so adding a stage meant
# remembering both and a browser-only link error was the failure mode. Now a new file in the directory is
# a new stage in BOTH targets, by construction.
RENDER_STAGE_SRCS := $(wildcard src/render/stages/*.cpp)
# CORE IS A COMPILE GROUP: core may name nothing above itself, and the one command that compiles
# clients, world and render together would hand it the world's include set instead. A wildcard, so a
# new core file is bound by $(INC_CORE) in all three targets without a list to remember.
CORE_TOP_SRCS := $(wildcard src/core/*.cpp)
CORE_IO_SRCS := $(wildcard src/core/io/*.cpp)
CORE_SRCS := $(CORE_TOP_SRCS) $(CORE_IO_SRCS)
# THE GENERATORS, and they are their own COMPILE GROUP for the same reason: everything else in a
# target is compiled with that target's include set, and a generator compiled with -Isrc/world could
# name the streamer in a build that stays green. A wildcard, so a new generator is bound by the
# include set in all three targets without a list to remember -- which is what `make
# verify-generators` compiles too.
GEN_SRCS := $(wildcard src/generators/*.cpp)
GEN_DRAW_SRCS := $(wildcard src/generators/draw/*.cpp)
# WHAT A PLANT DECLARATION IS, named once: the form and the species that rides it. The tree bench
# links the drawing half against these two alone and against no other generator, so it needs the
# pair spelled out where the wildcards above are too wide.
PLANT_DECL_SRCS := src/generators/TreeSpecies.cpp src/generators/GrowthForm.cpp
# ...and the same statement one level up, for Renderer and its neighbours: hand-listing them meant a
# new render/ file linked in one target and not the other.
RENDER_TOP_SRCS := $(wildcard src/render/*.cpp)
RENDER_SRCS := $(RENDER_TOP_SRCS) $(RENDER_STAGE_SRCS)

# WORLD, whole, and a wildcard for the same reason core is: a new file in the directory is bound by
# the directory's include set in all three targets without a list to remember. TerrainLoader and
# TilePool additionally see the tile server's public header.
WORLD_SRCS := $(wildcard src/world/*.cpp)
TERRAIN_SRCS := $(wildcard src/world/terrain/*.cpp)

# THE SIMULATION HALF OF THE CLIENTS. Nothing in this list may name render/, which `make world`
# proves by building it with $(INC_SIMHALF) and no renderer object at all.
SIM_SRCS := src/clients/Sim.cpp src/clients/Scene.cpp src/clients/Mod.cpp \
  src/clients/Animation.cpp src/clients/LogSinks.cpp src/clients/StreamTelemetry.cpp \
  src/clients/EyeTelemetry.cpp \
  src/clients/Species.cpp src/clients/RegionForge.cpp

# THE PICTURE HALF over it. Outshine owns the renderer and is the only thing in the tree that builds
# a scene; a client adds an entry point and an output medium over it. Both clients link this list,
# which is what `make verify-clients` holds them to.
APP_SRCS := src/clients/Outshine.cpp src/clients/Snapshot.cpp \
  src/clients/SceneRunner.cpp src/clients/SubjectBench.cpp src/clients/Png.cpp \
  src/clients/StandField.cpp \
  src/clients/FileArtifacts.cpp src/clients/HttpPost.cpp src/clients/ServerLog.cpp \
  src/clients/ServerTelemetry.cpp src/clients/FrameTelemetry.cpp \
  src/clients/MemoryTelemetry.cpp \
  src/clients/ServerArtifacts.cpp

# Named, not `build/obj-<target>/*.o`: the glob would link whatever a PREVIOUS target had left in
# there. One function of the object directory, so the three targets name the same six groups.
OBJS = $(patsubst src/core/%.cpp,$(1)/core-%.o,$(CORE_TOP_SRCS)) \
  $(patsubst src/core/io/%.cpp,$(1)/core-%.o,$(CORE_IO_SRCS)) \
  $(patsubst src/generators/%.cpp,$(1)/gen-%.o,$(GEN_SRCS)) \
  $(patsubst src/world/%.cpp,$(1)/world-%.o,$(WORLD_SRCS)) \
  $(patsubst src/world/terrain/%.cpp,$(1)/world-%.o,$(TERRAIN_SRCS)) \
  $(patsubst src/clients/%.cpp,$(1)/sim-%.o,$(SIM_SRCS))
# What a TilePool needs and nothing above it: the gate links the same objects `make wasm` built.
GATE_WASM_OBJS = $(patsubst src/core/%.cpp,build/obj-wasm/core-%.o,$(CORE_TOP_SRCS)) \
  $(patsubst src/core/io/%.cpp,build/obj-wasm/core-%.o,$(CORE_IO_SRCS)) \
  $(patsubst src/world/%.cpp,build/obj-wasm/world-%.o,$(WORLD_SRCS)) \
  $(patsubst src/world/terrain/%.cpp,build/obj-wasm/world-%.o,$(TERRAIN_SRCS))
PICTURE_OBJS = $(patsubst src/generators/draw/%.cpp,$(1)/gen-%.o,$(GEN_DRAW_SRCS)) \
  $(patsubst src/render/%.cpp,$(1)/render-%.o,$(RENDER_TOP_SRCS)) \
  $(patsubst src/render/stages/%.cpp,$(1)/render-%.o,$(RENDER_STAGE_SRCS)) \
  $(patsubst src/clients/%.cpp,$(1)/app-%.o,$(APP_SRCS))

# THE MODS, whole. A mod is a set of declared scenes and nothing else; the browser gets the same
# directory the native oracle reads off disk, so `<mod> <scene>` means the same two words in both.
MODS_DIR := ../mods

.PHONY: help wasm walk world treebench verify-generators verify-world verify-clients verify-counters image up down restart clean

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

wasm:            ## build the WASM client -> web/gpu.{js,wasm}
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  $(FB_UPTODATE) \
	  mkdir -p build/obj-wasm; \
	  EM="emcc $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) $(WASM_THREADS)"; \
	  EMPP="emcc -std=c++20 -O2 $(EMCC_WARN) $(DEPFLAGS) $(WASM_THREADS) -fexceptions --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -isystem vendor"; \
	  for f in $(TERRAIN_SRCS); do o=build/obj-wasm/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_WORLD) -Isrc/core $(INC_TILES) -Ivendor -c -o "$$o"; done; \
	  for f in $(WORLD_SRCS); do o=build/obj-wasm/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_WORLD) $(INC_TILES) -c -o "$$o"; done; \
	  for f in $(CORE_SRCS); do o=build/obj-wasm/core-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_CORE) -c -o "$$o"; done; \
	  for f in $(GEN_SRCS); do o=build/obj-wasm/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_GENERATORS) -c -o "$$o"; done; \
	  for f in $(GEN_DRAW_SRCS); do o=build/obj-wasm/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_DRAW) -c -o "$$o"; done; \
	  for f in $(SIM_SRCS); do o=build/obj-wasm/sim-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_SIMHALF) $(INC_TILES) -c -o "$$o"; done; \
	  for f in $(RENDER_SRCS); do o=build/obj-wasm/render-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EMPP "$$f" $(INC_RENDER) -c -o "$$o"; done; \
	  for f in $(APP_SRCS); do o=build/obj-wasm/app-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EMPP "$$f" $(INC_CLIENTS) $(INC_TILES) -c -o "$$o"; done; \
	  emcc $(call OBJS,build/obj-wasm) $(call PICTURE_OBJS,build/obj-wasm) \
	    src/clients/AppWasm.cpp src/clients/Walker.cpp \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O2 $(EMCC_WARN) -fexceptions \
	    $(INC_CLIENTS) -isystem vendor \
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
	  CC="c++ $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS)"; \
	  CCPP="c++ -std=c++20 -O2 $(CXX_WARN) $(DEPFLAGS) -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor"; \
	  for f in $(TERRAIN_SRCS); do o=build/obj-walk/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_WORLD) $(INC_TILES) -isystem vendor -c -o "$$o"; done; \
	  for f in $(WORLD_SRCS); do o=build/obj-walk/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_WORLD) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  for f in $(CORE_SRCS); do o=build/obj-walk/core-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_CORE) -c -o "$$o"; done; \
	  for f in $(GEN_SRCS); do o=build/obj-walk/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_GENERATORS) -c -o "$$o"; done; \
	  for f in $(GEN_DRAW_SRCS); do o=build/obj-walk/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_DRAW) -c -o "$$o"; done; \
	  for f in $(SIM_SRCS); do o=build/obj-walk/sim-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_SIMHALF) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  for f in $(RENDER_SRCS); do o=build/obj-walk/render-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CCPP "$$f" $(INC_RENDER) -c -o "$$o"; done; \
	  for f in $(APP_SRCS); do o=build/obj-walk/app-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CCPP "$$f" $(INC_CLIENTS) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  c++ src/clients/AppWalk.cpp \
	    $(call OBJS,build/obj-walk) $(call PICTURE_OBJS,build/obj-walk) \
	    -std=c++20 -O2 $(CXX_WARN) \
	    $(INC_CLIENTS) \
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
	  CC="c++ $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS)"; \
	  for f in $(TERRAIN_SRCS); do o=build/obj-world/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_WORLD) $(INC_TILES) -isystem vendor -c -o "$$o"; done; \
	  for f in $(WORLD_SRCS); do o=build/obj-world/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_WORLD) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  for f in $(CORE_SRCS); do o=build/obj-world/core-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_CORE) -c -o "$$o"; done; \
	  for f in $(GEN_SRCS); do o=build/obj-world/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_GENERATORS) -c -o "$$o"; done; \
	  for f in $(SIM_SRCS); do o=build/obj-world/sim-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_SIMHALF) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  c++ src/clients/WorldMain.cpp $(call OBJS,build/obj-world) \
	    -std=c++20 -O2 $(CXX_WARN) $(INC_SIMHALF) $(INC_TILES) \
	    -isystem vendor -isystem $(CURL_COMPAT) \
	    -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm \
	    -o build/fb_world; \
	  echo "world -> build/fb_world"

treebench:       ## grow every declared plant and measure it -> build/treebench (CSV on stdout)
	@cd $(SELF_DIR); set -e; \
	  mkdir -p build; \
	  c++ src/clients/TreeBench.cpp $(GEN_DRAW_SRCS) $(PLANT_DECL_SRCS) src/core/Json.cpp \
	    $(CXXSTD) -O2 $(CXX_WARN) $(INC_DRAW) \
	    -o build/treebench; \
	  echo "treebench -> build/treebench"

# WHAT MUST NOT BE REACHABLE FROM A GENERATOR. Each negative is compiled and has to fail FOR THE
# STATED REASON: any compile error would pass a bare exit-code test, including a typo in the gate.
GEN_NEGATIVES := RendererIsNotReachable WorldIsNotReachable LogIsNotReachable DrawIsNotReachable

verify-generators: ## a generators/ TU compiles against core and CANNOT name Renderer, World or Log
	@cd $(SELF_DIR); set -e; \
	  mkdir -p build; \
	  c++ src/generators/gate/CoreIsReachable.cpp $(CXXSTD) $(CXX_WARN) $(INC_GENERATORS) -fsyntax-only; \
	  for f in $(GEN_SRCS); do \
	    c++ "$$f" $(CXXSTD) $(CXX_WARN) $(INC_GENERATORS) -fsyntax-only; \
	    n=$$(c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -MM | tr ' \\' '\n\n' | grep -c '^src/'); \
	    up=$$(c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -MM | tr ' \\' '\n\n' | grep -c '^src/\(render\|world\)/' || true); \
	    if [ "$$up" != "0" ]; then echo "verify-generators: $$f includes $$up file(s) of render/ or world/" >&2; exit 1; fi; \
	    echo "verify-generators: $$f closure=$$n render+world=$$up"; done; \
	  c++ src/generators/gate/DrawIsReachable.cpp $(CXXSTD) $(CXX_WARN) $(INC_DRAW) -fsyntax-only; \
	  for f in $(GEN_DRAW_SRCS); do \
	    c++ "$$f" $(CXXSTD) $(CXX_WARN) $(INC_DRAW) -fsyntax-only; \
	    n=$$(c++ "$$f" $(CXXSTD) $(INC_DRAW) -MM | tr ' \\' '\n\n' | grep -c '^src/'); \
	    up=$$(c++ "$$f" $(CXXSTD) $(INC_DRAW) -MM | tr ' \\' '\n\n' | grep -c '^src/\(render\|world\|clients\)/' || true); \
	    if [ "$$up" != "0" ]; then echo "verify-generators: $$f includes $$up file(s) of render/ or world/" >&2; exit 1; fi; \
	    echo "verify-generators: $$f closure=$$n render+world=$$up"; done; \
	  for g in $(GEN_NEGATIVES); do f=src/generators/gate/$$g.cpp; \
	    if c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -fsyntax-only 2>/dev/null; then \
	      echo "verify-generators: $$f COMPILED -- the include set no longer bounds generators/" >&2; exit 1; \
	    fi; \
	    if ! c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -fsyntax-only 2>&1 | grep -q "file not found"; then \
	      echo "verify-generators: $$f failed for some OTHER reason than an unreachable header" >&2; exit 1; \
	    fi; done; \
	  c++ src/generators/gate/SameRegionSamePlacement.cpp $(GEN_SRCS) src/core/ClassStructure.cpp \
	    src/core/Json.cpp src/core/AlpineLimit.cpp \
	    $(CXXSTD) -O2 $(CXX_WARN) $(INC_GENERATORS) -o build/gen_gate; \
	  ./build/gen_gate; \
	  echo "verify-generators: core reachable, render/ world/ log and draw have no spelling in generators/"

# WHAT THE TYPE SYSTEM MUST REFUSE. A rule that only a reviewer enforces is a rule that comes back:
# both of these were live defects before they were compile errors. The negative halves are built with
# the house warning set because -Werror is what turns [[nodiscard]] into a refusal.
verify-world:    ## a world/ TU compiles against core and CANNOT name a generator or the renderer
	@cd $(SELF_DIR); set -e; \
	  for f in $(WORLD_SRCS); do \
	    up=$$(c++ "$$f" $(CXXSTD) $(INC_WORLD) $(INC_TILES) -isystem vendor -isystem $(CURL_COMPAT) -MM | tr ' \\' '\n\n' | grep -c '^src/\(render\|generators\|clients\)/' || true); \
	    if [ "$$up" != "0" ]; then echo "verify-world: $$f includes $$up file(s) above world/" >&2; exit 1; fi; done; \
	  f=src/world/gate/GeneratorIsNotReachable.cpp; \
	  if c++ "$$f" $(CXXSTD) $(INC_WORLD) -fsyntax-only 2>/dev/null; then \
	    echo "verify-world: $$f COMPILED -- world/ can name a generator" >&2; exit 1; \
	  fi; \
	  if ! c++ "$$f" $(CXXSTD) $(INC_WORLD) -fsyntax-only 2>&1 | grep -q "file not found"; then \
	    echo "verify-world: $$f failed for some OTHER reason than an unreachable header" >&2; exit 1; \
	  fi; \
	  echo "verify-world: generators/ and render/ have no spelling in world/"

# THE ONE COUNTER TEST THAT CANNOT BE TAKEN ON THE ORACLE. `long` is 64 bits natively and 32 on
# wasm32, so a cumulative counter declared `long` wraps only where the code ships -- and every
# acceptance CSV in the archive is written by the native oracle. This drives the pool's fastest
# accumulator past 2^31 IN THE BROWSER and reads it back through the telemetry's own push.
verify-counters: ## a cumulative counter carries past 2^31 on wasm32, in the browser
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  emcc src/world/gate/CountersDoNotWrap.cpp $(GATE_WASM_OBJS) \
	    $(CXXSTD) -O2 $(EMCC_WARN) $(WASM_THREADS) $(INC_WORLD) $(INC_TILES) \
	    $(WASM_THREADS) -sPTHREAD_POOL_SIZE=2 -sINITIAL_MEMORY=128MB -sSTACK_SIZE=256KB \
	    -sDEFAULT_PTHREAD_STACK_SIZE=256KB -sABORTING_MALLOC=0 -sEXIT_RUNTIME=1 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_fb_take_http_body \
	    -o web/counters.html; \
	  node tools/browser_gate.cjs /counters.html

verify-types:    ## a value whose misuse must not compile -- and does not
	@cd $(SELF_DIR); set -e; \
	  for f in src/core/gate/GroundSampleIsUsable.cpp src/core/gate/WaterDepthIsUsable.cpp; do \
	    c++ "$$f" $(CXXSTD) $(CXX_WARN) -Isrc/core -fsyntax-only; done; \
	  for f in src/core/gate/HeightIsNotReachableWithoutItsState.cpp src/core/gate/AnswerIsNotIgnorable.cpp \
	           src/core/gate/DepthIsNeverNegative.cpp; do \
	    if c++ "$$f" $(CXXSTD) $(CXX_WARN) -Isrc/core -fsyntax-only 2>/dev/null; then \
	      echo "verify-types: $$f COMPILED -- a number is readable without the state that gives it meaning" >&2; exit 1; \
	    fi; done; \
	  echo "verify-types: GroundSample and WaterDepth usable, their metres unreachable without the state"

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
	  build/gpu_walk build/fb_world build/gen_gate \
	  web/gpu.js web/gpu.wasm web/gpu.data
