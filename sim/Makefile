# fb-sim — the outshine Command Center. Self-contained: this Makefile, Dockerfile, the C++ source
# under src/, and the vendored toolchain under vendor/ (Dawn, emdawnwebgpu, stb, build scripts) all
# live under this one directory. The only cross-directory reference is fb-tiles' public bake-version
# header (../tiles/src/style_ver.h).
#
#   make wasm     build the WASM client -> web/gpu.js + web/gpu.wasm
#   make walk     build the native pedestrian frame oracle (Dawn + fb-tiles terrain) -> build/gpu_walk
#   make world    build the server target: everything EXCEPT render/, no device -> build/fb_world
#   make gates    run every gate a round must not break, one line each, non-zero on any failure --
#                 its green is what a "done when" means, and nothing else obliges a round to type
#                 the negatives one at a time
#   make gates-build  the subset a compiler and a refusing program decide, with no browser and no
#                 traversal -- what an edit can afford between commits
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

# THE INSTRUMENTS OF THE DECLARED SANITISED RUNS, and they are never in a shipping target: `walk` and
# `wasm` do not name them, so the frame cost of this line is zero by construction.
# -fno-sanitize-recover is what makes UndefinedBehaviorSanitizer a gate rather than a log: without it
# a signed overflow prints and the run exits 0. -g1 buys the line tables a report is worthless
# without; -O2 stays because a sanitised run at another optimisation level measures another program.
SAN_NATIVE := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1
SAN_WASM   := -fsanitize=address -fno-omit-frame-pointer -g1

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

.PHONY: help wasm wasm-asan walk walk-asan world treebench gates gates-build verify-generators \
  verify-world verify-clients verify-counters verify-types verify-refusals verify-walk verify-wasm \
  verify-walk-asan verify-wasm-asan image up down restart clean

# WHERE A GATE'S OUTPUT GOES. Outside the tree on purpose: a log nobody committed on purpose is a
# file the next round has to make a decision about.
GATE_DIR := $(shell echo $${TMPDIR:-/tmp})/outshine-gates

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

# ONE WASM BUILD, TWO CONFIGURATIONS -- the same statement the native define makes, for the target
# that ships. The sanitised client is a SECOND page: it must never overwrite web/gpu.js, because the
# host serves that file to whoever is measuring.
#   $(1) object directory   $(2) extra compile and link flags   $(3) output js   $(4) client name
define WASM_BUILD
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  $(FB_UPTODATE) \
	  mkdir -p $(1); \
	  EM="emcc $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) $(WASM_THREADS) $(2)"; \
	  EMPP="emcc -std=c++20 -O2 $(EMCC_WARN) $(DEPFLAGS) $(WASM_THREADS) $(2) -fexceptions --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -isystem vendor"; \
	  for f in $(TERRAIN_SRCS); do o=$(1)/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_WORLD) -Isrc/core $(INC_TILES) -Ivendor -c -o "$$o"; done; \
	  for f in $(WORLD_SRCS); do o=$(1)/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_WORLD) $(INC_TILES) -c -o "$$o"; done; \
	  for f in $(CORE_SRCS); do o=$(1)/core-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_CORE) -c -o "$$o"; done; \
	  for f in $(GEN_SRCS); do o=$(1)/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_GENERATORS) -c -o "$$o"; done; \
	  for f in $(GEN_DRAW_SRCS); do o=$(1)/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_DRAW) -c -o "$$o"; done; \
	  for f in $(SIM_SRCS); do o=$(1)/sim-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EM "$$f" $(INC_SIMHALF) $(INC_TILES) -c -o "$$o"; done; \
	  for f in $(RENDER_SRCS); do o=$(1)/render-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EMPP "$$f" $(INC_RENDER) -c -o "$$o"; done; \
	  for f in $(APP_SRCS); do o=$(1)/app-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$EMPP "$$f" $(INC_CLIENTS) $(INC_TILES) -c -o "$$o"; done; \
	  emcc $(call OBJS,$(1)) $(call PICTURE_OBJS,$(1)) \
	    src/clients/AppWasm.cpp src/clients/Walker.cpp \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O2 $(EMCC_WARN) -fexceptions \
	    $(INC_CLIENTS) -isystem vendor $(2) -DOUTSHINE_CLIENT="\"$(4)\"" \
	    $(WASM_THREADS_LINK) $(WASM_MAIN_MEMORY) -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_fb_take_http_body \
	    --preload-file web/moon.jpg@/moon.jpg \
	    --preload-file $(MODS_DIR)@/mods \
	    --preload-file assets/world/vegetation.json@/vegetation.json \
	    --preload-file assets/world/species@/assets/world/species \
	    --preload-file assets/world/ground-materials.json@/ground-materials.json \
	    -o $(3); \
	  echo "-> $(3) (mods: $(MODS_DIR))"
endef

wasm:            ## build the WASM client -> web/gpu.{js,wasm}
	$(call WASM_BUILD,build/obj-wasm,,web/gpu.js,wasm)

# FB_BUILD is blanked because it would otherwise be WRONG: the host's wasm_build_id hashes
# web/gpu.wasm whatever page asked for it (clients/SimHost.cpp:151), so a second page inherits the
# shipping module's identity. A blank field says "this run is not pinned"; a hash would say something
# untrue. doc/bugs.md carries what the fix is.
wasm-asan:       ## build the same client under address -> web/gpu-asan.{js,wasm} + web/asan.html
	$(call WASM_BUILD,build/obj-wasm-asan,$(SAN_WASM),web/gpu-asan.js,wasm)
	@cd $(SELF_DIR); set -e; \
	  sed -e 's|src="gpu.js"|src="gpu-asan.js"|' \
	    -e 's|<script src="config.js"></script>|<script src="config.js"></script><script>window.FB_BUILD="";</script>|' \
	    web/index.html > web/asan.html; \
	  if ! grep -q 'src="gpu-asan.js"' web/asan.html; then \
	    echo "wasm-asan: the rewrite matched nothing -- web/index.html spells its module tag differently, and sed says nothing when it matches nothing" >&2; exit 1; fi; \
	  if grep -q 'src="gpu.js"' web/asan.html; then \
	    echo "wasm-asan: asan.html still loads the SHIPPING module -- the sanitiser would be reported silent over a module that carries none" >&2; exit 1; fi; \
	  if ! grep -q 'window.FB_BUILD=""' web/asan.html; then \
	    echo "wasm-asan: asan.html would inherit web/gpu.wasm's hash as its own identity" >&2; exit 1; fi; \
	  echo "-> web/asan.html (module gpu-asan.js, identity blanked)"

# ONE NATIVE BUILD, TWO CONFIGURATIONS: the shipping oracle and the sanitised run differ by a flag
# set and an object directory and by NOTHING else. A second copy of this recipe is how the two drift,
# and a sanitised build that has drifted measures a program which does not ship. The object
# directories are separate so an instrumented object can never be linked into the shipping binary.
#   $(1) object directory   $(2) extra compile and link flags   $(3) output binary   $(4) client name
define NATIVE_BUILD
	@cd $(SELF_DIR); set -e; \
	  $(FB_UPTODATE) \
	  PLATFORM_LIBS=""; \
	  if [ "$$(uname -s)" = "Darwin" ]; then \
	    PLATFORM_LIBS="-framework Cocoa -framework IOKit -framework Foundation -framework IOSurface -framework QuartzCore -framework Metal"; \
	  fi; \
	  test -f $(DAWN_LIBDIR)/libwebgpu_dawn.a || bash vendor/build_dawn_native.sh; \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  mkdir -p $(1); \
	  CC="c++ $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS) $(2)"; \
	  CCPP="c++ -std=c++20 -O2 $(CXX_WARN) $(DEPFLAGS) $(2) -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor"; \
	  for f in $(TERRAIN_SRCS); do o=$(1)/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_WORLD) $(INC_TILES) -isystem vendor -c -o "$$o"; done; \
	  for f in $(WORLD_SRCS); do o=$(1)/world-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_WORLD) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  for f in $(CORE_SRCS); do o=$(1)/core-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_CORE) -c -o "$$o"; done; \
	  for f in $(GEN_SRCS); do o=$(1)/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_GENERATORS) -c -o "$$o"; done; \
	  for f in $(GEN_DRAW_SRCS); do o=$(1)/gen-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_DRAW) -c -o "$$o"; done; \
	  for f in $(SIM_SRCS); do o=$(1)/sim-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CC "$$f" $(INC_SIMHALF) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  for f in $(RENDER_SRCS); do o=$(1)/render-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CCPP "$$f" $(INC_RENDER) -c -o "$$o"; done; \
	  for f in $(APP_SRCS); do o=$(1)/app-$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || $$CCPP "$$f" $(INC_CLIENTS) $(INC_TILES) -isystem $(CURL_COMPAT) -c -o "$$o"; done; \
	  c++ src/clients/AppWalk.cpp \
	    $(call OBJS,$(1)) $(call PICTURE_OBJS,$(1)) \
	    -std=c++20 -O2 $(CXX_WARN) $(2) -DOUTSHINE_CLIENT="\"$(4)\"" \
	    $(INC_CLIENTS) \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor \
	    -isystem $(CURL_COMPAT) \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm $$PLATFORM_LIBS \
	    -o $(3); \
	  echo "-> $(3)"
endef

walk:            ## build the pedestrian frame oracle (render+world) -> build/gpu_walk
	$(call NATIVE_BUILD,build/obj-walk,,build/gpu_walk,gpu_walk)

# THE SAME CLIENT, NOT A SECOND ONE, so it carries the same client name: what separates its rows from
# the shipping oracle's is the instrument field, which src/clients/Sanitisers.h takes from the
# compiler and no build can state wrongly.
walk-asan:       ## build the same oracle under address+undefined -> build/gpu_walk_asan
	$(call NATIVE_BUILD,build/obj-walk-asan,$(SAN_NATIVE),build/gpu_walk_asan,gpu_walk)

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

# WHAT A PROGRAM MUST REFUSE ONCE IT IS RUNNING. A compile gate cannot reach either of these: a bench
# with nothing to measure printed a header and exited 0 until 2026-08-11, which is a run that reports
# success over an empty directory, and an unknown growth form must not be a plant that grows anyway.
verify-refusals: treebench ## a bench with nothing to measure refuses, and so does an unknown growth form
	@cd $(SELF_DIR); set -e; \
	  d=$(GATE_DIR)/refusals; rm -rf "$$d"; mkdir -p "$$d/empty" "$$d/badform"; \
	  if ./build/treebench --assets "$$d/empty" >/dev/null 2>"$$d/empty.err"; then \
	    echo "verify-refusals: treebench measured an empty directory and exited 0" >&2; exit 1; fi; \
	  grep -q "$$d/empty" "$$d/empty.err" || { \
	    echo "verify-refusals: the bench refused without naming where it looked" >&2; exit 1; }; \
	  sed 's|"form": "[a-z_]*"|"form": "no_such_form"|' assets/world/species/ash.json > "$$d/badform/ash.json"; \
	  grep -q '"no_such_form"' "$$d/badform/ash.json" || { \
	    echo "verify-refusals: the fixture carries no form field -- the gate would pass on nothing" >&2; exit 1; }; \
	  if ./build/treebench --assets "$$d/badform" >/dev/null 2>"$$d/badform.err"; then \
	    echo "verify-refusals: a declaration with an unknown form grew anyway" >&2; exit 1; fi; \
	  grep -q "no_such_form" "$$d/badform.err" || { \
	    echo "verify-refusals: the refusal does not name the value it refused" >&2; exit 1; }; \
	  echo "verify-refusals: an empty bench and an unknown form both refuse, and both name why"

# THE TWO SHIPPING TRANSLATIONS. Neither is covered by any gate above: the sanitised walk compiles
# every source natively but under another flag set, and verify-counters compiles world+core alone
# under emcc. What actually ships is the C++20 / -fexceptions / emdawnwebgpu translation of render/
# and clients/, and it was the one translation no gate ever performed.
verify-wasm: wasm ## the module that ships links -- render/ and clients/ through emcc
	@cd $(SELF_DIR); set -e; \
	  if [ ! -s web/gpu.wasm ] || [ ! -s web/gpu.js ]; then \
	    echo "verify-wasm: the emcc link produced no module" >&2; exit 1; fi; \
	  echo "verify-wasm: web/gpu.wasm $$(shasum -a 256 web/gpu.wasm | cut -c1-16) $$(wc -c < web/gpu.wasm | tr -d ' ') B"

verify-walk: walk ## the oracle that measures links -- the same sources against native Dawn
	@cd $(SELF_DIR); set -e; \
	  if [ ! -x build/gpu_walk ]; then echo "verify-walk: the link produced no oracle" >&2; exit 1; fi; \
	  echo "verify-walk: build/gpu_walk $$(shasum -a 256 build/gpu_walk | cut -c1-16)"

# WHAT demo/walk-500 DRAWS, taken from the unsanitised oracle on this build. A binary that exits 0
# having drawn nothing satisfies an exit code and nothing else, and so does a run that walked some
# other world. Asserting these is what makes "the sanitiser was silent over 10 800 frames" a
# statement about THIS traversal.
WALK500_FRAMES := 10800
WALK500_DRAWN  := impostorStands=9565 treeTris=19130

# THE DECLARED SANITISED RUNS, and what they decide is whether the sanitiser speaks -- never whether
# the frame holds, because an instrumented frame is not the shipping frame. The shipping targets name
# neither $(SAN_NATIVE) nor $(SAN_WASM) and the objects live in their own directories, so no
# instrumented object can reach a binary anyone measures.
verify-walk-asan: walk-asan ## demo/walk-500 over its whole traversal under address+undefined, natively
	@cd $(SELF_DIR); mkdir -p $(GATE_DIR)/walk-asan; o=$(GATE_DIR)/walk-asan.out; \
	  if ! OUTSHINE_OUT=$(GATE_DIR)/walk-asan ./build/gpu_walk_asan demo walk-500 >"$$o" 2>&1; then \
	    tail -40 "$$o" >&2; echo "verify-walk-asan: the sanitised run did not finish ($$o)" >&2; exit 1; fi; \
	  if grep -qE "AddressSanitizer|runtime error:" "$$o"; then \
	    grep -nE "AddressSanitizer|runtime error:" "$$o" >&2; \
	    echo "verify-walk-asan: the run finished but the sanitiser spoke ($$o)" >&2; exit 1; fi; \
	  v=$$(grep -F "INFO run motion path=walk-500.csv" "$$o" | tail -1); \
	  if [ -z "$$v" ]; then \
	    echo "verify-walk-asan: the run exited 0 and wrote no motion verdict ($$o)" >&2; exit 1; fi; \
	  case "$$v" in *" frames=$(WALK500_FRAMES) "*) ;; *) \
	    echo "$$v" >&2; \
	    echo "verify-walk-asan: the traversal was not $(WALK500_FRAMES) frames" >&2; exit 1;; esac; \
	  case "$$v" in *"$(WALK500_DRAWN)"*) ;; *) \
	    echo "$$v" >&2; \
	    echo "verify-walk-asan: the sanitised run drew a DIFFERENT world -- expected $(WALK500_DRAWN)" >&2; exit 1;; esac; \
	  echo "verify-walk-asan: $(WALK500_FRAMES) frames drew $(WALK500_DRAWN), address+undefined silent"

# Not in $(GATES), and the reason is measured rather than budgetary: it does not reach the end of a
# ONE-frame scene on this host. doc/bugs.md carries the numbers.
verify-wasm-asan: wasm-asan ## run demo/walk-500 under address in the browser
	@cd $(SELF_DIR) && OUTSHINE_PAGE=/asan.html node tools/browser_run.cjs demo walk-500 3600

# EVERY GATE A ROUND MUST NOT BREAK, IN ONE COMMAND. The negatives were .PHONY and nothing obliged a
# round to type them -- the failure mode of a debug configuration nobody builds. Every gate runs even
# after one has fallen, because the second failure is information the first one would have hidden.
#
# THE SPLIT IS WHAT A GATE DECIDES, NOT WHEN IT IS RUN. $(GATES_BUILD) is everything a compiler and a
# refusing program can settle: the layering, what the type system must refuse, what a running program
# must refuse, and both shipping links. $(GATES_RUN) is what only a client that RUNS can settle -- a
# counter that wraps on wasm32 and nowhere else, and a sanitiser that speaks only over a traversal.
# The second set needs a browser and a 10 800-frame walk and costs two orders of magnitude more, so
# the first one exists to be affordable in an edit: a gate that is skipped is not a defence.
GATES_BUILD := verify-generators verify-clients verify-types verify-world verify-refusals \
  verify-wasm verify-walk
GATES_RUN := verify-counters verify-walk-asan
GATES := $(GATES_BUILD) $(GATES_RUN)

# make marks a recipe line as recursive by finding the LITERAL text `$(MAKE)` in it before expanding
# it, and a line so marked is executed even under -n. Naming the same variable something else means
# the line is printed and not run under -n, and runs normally without it.
SUB := $(MAKE)

#   $(1) the gate list   $(2) what to call the set in the verdict
define RUN_GATES
	@cd $(SELF_DIR); mkdir -p $(GATE_DIR); red=0; \
	  for g in $(1); do \
	    t0=$$(date +%s); \
	    if $(SUB) --no-print-directory "$$g" >"$(GATE_DIR)/$$g.log" 2>&1; then v=PASS; else v=FAIL; red=1; fi; \
	    printf '%-4s %-18s %5ds  %s\n' "$$v" "$$g" "$$(( $$(date +%s) - t0 ))" "$(GATE_DIR)/$$g.log"; \
	  done; \
	  if [ "$$red" != 0 ]; then echo "$(2): RED"; exit 1; fi; \
	  echo "$(2): green"
endef

gates:           ## run every gate a round must not break -- one line each, non-zero on any failure
	$(call RUN_GATES,$(GATES),gates)

gates-build:     ## the gates a compiler and a refusal decide -- no browser, no traversal
	$(call RUN_GATES,$(GATES_BUILD),gates-build)

image:          ## build the deployable fb-sim web container (context = sim/, trimmed by .containerignore)
	cd $(SELF_DIR) && podman build -f Dockerfile -t fb-sim .

up:              ## run the web host container (fb-sim, :8080; sim runs IN the browser)
	@cd $(SELF_DIR) && bash up.sh

down:            ## stop the web host
	-podman rm -f fb-sim 2>/dev/null

restart: down up ## stop and start the web host

clean:           ## remove build artifacts (WASM + walk + world + the sanitised runs)
	cd $(SELF_DIR) && rm -rf build/obj-wasm build/obj-walk build/obj-world \
	  build/obj-wasm-asan build/obj-walk-asan build/gpu_walk_asan \
	  build/gpu_walk build/fb_world build/gen_gate \
	  web/gpu.js web/gpu.wasm web/gpu.data \
	  web/gpu-asan.js web/gpu-asan.wasm web/gpu-asan.data web/asan.html
