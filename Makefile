# Outshine — the library under src/, the tests under test/, the tile server under tiles/. This
# Makefile, the vendored toolchain under vendor/ (Dawn, stb, build scripts) and the declarations
# under assets/ and mods/ live beside them. The only cross-project reference is fb-tiles' public
# bake-version header (tiles/src/style_ver.h).
#
#   make walk     build the interactive client / frame oracle (test/clients/AppWalk.cpp) -> build/gpu_walk
#   make world    build the headless target: everything EXCEPT render/, no device -> build/fb_world
#   make gates    run every gate a round must not break, one line each, non-zero on any failure --
#                 its green is what a "done when" means, and nothing else obliges a round to type
#                 the negatives one at a time
#   make gates-build  the subset a compiler and a refusing program decide, with no traversal --
#                 what an edit can afford between commits
#   make verify-generators  a generators/ translation unit can name core and NOTHING above it --
#                 the include set is the layering, and a breach is a compile error
#   make verify-clients  an entry point is an entry point and nothing else, and the scene is built
#                 in exactly one place
#   make clean    remove build artifacts
SHELL := /bin/bash
.DEFAULT_GOAL := help

# This Makefile's own directory, absolute -- recipes cd here so every path is repository-relative no
# matter where make was invoked from.
SELF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

CXX_WARN  := -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
CXXSTD    := -std=c++17

# THE INSTRUMENTS OF THE DECLARED SANITISED RUNS, and they are never in a shipping target: `walk`
# does not name them, so the frame cost of this line is zero by construction.
# -fno-sanitize-recover is what makes UndefinedBehaviorSanitizer a gate rather than a log: without it
# a signed overflow prints and the run exits 0. -g1 buys the line tables a report is worthless
# without; -O2 stays because a sanitised run at another optimisation level measures another program.
SAN_NATIVE := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1

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
# file between these directories touches no source line). THE INCLUDE SET IS THE LAYERING, and the
# targets are what enforce it: core = shared value types and services; render = Renderer + the WebGPU
# stages; world = World/tile streaming, with world/terrain the lean terrain API under it; clients =
# what a scene is run through. `make world` links without -Isrc/render at all, so an upward include
# from world/ is a build failure rather than a rule.
# ONE COMPILE GROUP PER LAYER, and the group is what the layering IS: every source is compiled with
# the include set of its own directory, so an upward include has no spelling rather than a rule. The
# targets link the same objects -- a set that is wider in one target than in another is a breach that
# builds.
INC_CORE    := -Isrc/core -Isrc/core/io
# A GENERATOR'S WHOLE WORLD. Not a subset of the others by accident: `make verify-generators`
# compiles against exactly this and asserts that Renderer, World and the log have no spelling in it.
# src/core/io is ABSENT on purpose -- Log and TelemetryBus live there, and a generator that cannot
# name them cannot do I/O.
INC_GENERATORS := -Isrc/core -Isrc/generators
# ...and the picture half of the same layer, which the headless target never compiles: DrawSource and
# its sink hand clusters and instances to a renderer, and that target has no renderer to hand them to.
INC_DRAW := $(INC_GENERATORS) -Isrc/generators/draw
INC_WORLD  := $(INC_CORE) -Isrc/world -Isrc/world/terrain
INC_RENDER := $(INC_CORE) -Isrc/render -Isrc/render/stages
# THE SIMULATION HALF OF THE CLIENTS, and it is the ONLY place that may name world and generators in
# one translation unit: -Isrc/render is absent, so the headless target's half cannot reach a renderer.
INC_SIMHALF := $(INC_CORE) -Isrc/world -Isrc/world/terrain -Isrc/generators -Isrc/clients
# ...and the picture half over it, the one set that holds everything.
INC_CLIENTS := $(INC_SIMHALF) -Isrc/generators/draw $(INC_RENDER)
INC_TILES := -Itiles/src

# EVERY WebGPU render stage, one class per shader (CLAUDE.md render/stages). A wildcard, not a hand
# list: a new file in the directory is a new stage in every target, by construction.
RENDER_STAGE_SRCS := $(wildcard src/render/stages/*.cpp)
# CORE IS A COMPILE GROUP: core may name nothing above itself, and the one command that compiles
# clients, world and render together would hand it the world's include set instead. A wildcard, so a
# new core file is bound by $(INC_CORE) in every target without a list to remember.
CORE_TOP_SRCS := $(wildcard src/core/*.cpp)
CORE_IO_SRCS := $(wildcard src/core/io/*.cpp)
CORE_SRCS := $(CORE_TOP_SRCS) $(CORE_IO_SRCS)
# THE GENERATORS, and they are their own COMPILE GROUP for the same reason: everything else in a
# target is compiled with that target's include set, and a generator compiled with -Isrc/world could
# name the streamer in a build that stays green. A wildcard, so a new generator is bound by the
# include set in every target without a list to remember -- which is what `make verify-generators`
# compiles too.
GEN_SRCS := $(wildcard src/generators/*.cpp)
GEN_DRAW_SRCS := $(wildcard src/generators/draw/*.cpp)
# WHAT A PLANT DECLARATION IS, named once: the form and the species that rides it. The tree bench
# links the drawing half against these two alone and against no other generator, so it needs the
# pair spelled out where the wildcards above are too wide.
PLANT_DECL_SRCS := src/generators/TreeSpecies.cpp src/generators/GrowthForm.cpp
# ...and the same statement one level up, for Renderer and its neighbours.
RENDER_TOP_SRCS := $(wildcard src/render/*.cpp)
RENDER_SRCS := $(RENDER_TOP_SRCS) $(RENDER_STAGE_SRCS)

# WORLD, whole, and a wildcard for the same reason core is: a new file in the directory is bound by
# the directory's include set in every target without a list to remember. TerrainLoader and
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
# a scene; a test adds an entry point and an output medium over it. That is what `make
# verify-clients` holds them to.
APP_SRCS := src/clients/Outshine.cpp src/clients/Snapshot.cpp \
  src/clients/SceneRunner.cpp src/clients/SubjectBench.cpp src/clients/Png.cpp \
  src/clients/StandField.cpp \
  src/clients/FileArtifacts.cpp src/clients/HttpPost.cpp src/clients/ServerLog.cpp \
  src/clients/ServerTelemetry.cpp src/clients/FrameTelemetry.cpp \
  src/clients/MemoryTelemetry.cpp src/clients/Walker.cpp

# Named, not `build/obj-<target>/*.o`: the glob would link whatever a PREVIOUS target had left in
# there. One function of the object directory, so the targets name the same six groups.
OBJS = $(patsubst src/core/%.cpp,$(1)/core-%.o,$(CORE_TOP_SRCS)) \
  $(patsubst src/core/io/%.cpp,$(1)/core-%.o,$(CORE_IO_SRCS)) \
  $(patsubst src/generators/%.cpp,$(1)/gen-%.o,$(GEN_SRCS)) \
  $(patsubst src/world/%.cpp,$(1)/world-%.o,$(WORLD_SRCS)) \
  $(patsubst src/world/terrain/%.cpp,$(1)/world-%.o,$(TERRAIN_SRCS)) \
  $(patsubst src/clients/%.cpp,$(1)/sim-%.o,$(SIM_SRCS))
PICTURE_OBJS = $(patsubst src/generators/draw/%.cpp,$(1)/gen-%.o,$(GEN_DRAW_SRCS)) \
  $(patsubst src/render/%.cpp,$(1)/render-%.o,$(RENDER_TOP_SRCS)) \
  $(patsubst src/render/stages/%.cpp,$(1)/render-%.o,$(RENDER_STAGE_SRCS)) \
  $(patsubst src/clients/%.cpp,$(1)/app-%.o,$(APP_SRCS))

.PHONY: help walk walk-asan world treebench gates gates-build verify-generators \
  verify-world verify-clients verify-types verify-refusals verify-walk \
  verify-still verify-walk-asan clean

# WHERE A GATE'S OUTPUT GOES. Outside the tree on purpose: a log nobody committed on purpose is a
# file the next round has to make a decision about.
GATE_DIR := $(shell echo $${TMPDIR:-/tmp})/outshine-gates

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

# ONE BUILD, TWO CONFIGURATIONS: the client and the sanitised run differ by a flag set and an object
# directory and by NOTHING else. A second copy of this recipe is how the two drift, and a sanitised
# build that has drifted measures a program which does not ship. The object directories are separate
# so an instrumented object can never be linked into the binary anyone measures.
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
	  c++ test/clients/AppWalk.cpp \
	    $(call OBJS,$(1)) $(call PICTURE_OBJS,$(1)) \
	    -std=c++20 -O2 $(CXX_WARN) $(2) -DOUTSHINE_CLIENT="\"$(4)\"" \
	    $(INC_CLIENTS) \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor \
	    -isystem $(CURL_COMPAT) \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm $$PLATFORM_LIBS \
	    -o $(3); \
	  echo "-> $(3)"
endef

walk:            ## build the interactive client / frame oracle (render+world) -> build/gpu_walk
	$(call NATIVE_BUILD,build/obj-walk,,build/gpu_walk,gpu_walk)

# THE SAME CLIENT, NOT A SECOND ONE, so it carries the same client name: what separates its rows from
# the measuring oracle's is the instrument field, which src/clients/Sanitisers.h takes from the
# compiler and no build can state wrongly.
walk-asan:       ## build the same client under address+undefined -> build/gpu_walk_asan
	$(call NATIVE_BUILD,build/obj-walk-asan,$(SAN_NATIVE),build/gpu_walk_asan,gpu_walk)

world:           ## build the headless target -- everything except render/, no device -> build/fb_world
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
	  c++ test/clients/WorldMain.cpp $(call OBJS,build/obj-world) \
	    -std=c++20 -O2 $(CXX_WARN) $(INC_SIMHALF) $(INC_TILES) \
	    -isystem vendor -isystem $(CURL_COMPAT) \
	    -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm \
	    -o build/fb_world; \
	  echo "world -> build/fb_world"

treebench:       ## grow every declared plant and measure it -> build/treebench (CSV on stdout)
	@cd $(SELF_DIR); set -e; \
	  mkdir -p build; \
	  c++ test/clients/TreeBench.cpp $(GEN_DRAW_SRCS) $(PLANT_DECL_SRCS) src/core/Json.cpp \
	    $(CXXSTD) -O2 $(CXX_WARN) $(INC_DRAW) \
	    -o build/treebench; \
	  echo "treebench -> build/treebench"

# WHAT MUST NOT BE REACHABLE FROM A GENERATOR. Each negative is compiled and has to fail FOR THE
# STATED REASON: any compile error would pass a bare exit-code test, including a typo in the gate.
GEN_NEGATIVES := RendererIsNotReachable WorldIsNotReachable LogIsNotReachable DrawIsNotReachable

verify-generators: ## a generators/ TU compiles against core and CANNOT name Renderer, World or Log
	@cd $(SELF_DIR); set -e; \
	  mkdir -p build; \
	  c++ test/generators/CoreIsReachable.cpp $(CXXSTD) $(CXX_WARN) $(INC_GENERATORS) -fsyntax-only; \
	  for f in $(GEN_SRCS); do \
	    c++ "$$f" $(CXXSTD) $(CXX_WARN) $(INC_GENERATORS) -fsyntax-only; \
	    n=$$(c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -MM | tr ' \\' '\n\n' | grep -c '^src/'); \
	    up=$$(c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -MM | tr ' \\' '\n\n' | grep -c '^src/\(render\|world\)/' || true); \
	    if [ "$$up" != "0" ]; then echo "verify-generators: $$f includes $$up file(s) of render/ or world/" >&2; exit 1; fi; \
	    echo "verify-generators: $$f closure=$$n render+world=$$up"; done; \
	  c++ test/generators/draw/DrawIsReachable.cpp $(CXXSTD) $(CXX_WARN) $(INC_DRAW) -fsyntax-only; \
	  for f in $(GEN_DRAW_SRCS); do \
	    c++ "$$f" $(CXXSTD) $(CXX_WARN) $(INC_DRAW) -fsyntax-only; \
	    n=$$(c++ "$$f" $(CXXSTD) $(INC_DRAW) -MM | tr ' \\' '\n\n' | grep -c '^src/'); \
	    up=$$(c++ "$$f" $(CXXSTD) $(INC_DRAW) -MM | tr ' \\' '\n\n' | grep -c '^src/\(render\|world\|clients\)/' || true); \
	    if [ "$$up" != "0" ]; then echo "verify-generators: $$f includes $$up file(s) of render/ or world/" >&2; exit 1; fi; \
	    echo "verify-generators: $$f closure=$$n render+world=$$up"; done; \
	  for g in $(GEN_NEGATIVES); do f=test/generators/$$g.cpp; \
	    if c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -fsyntax-only 2>/dev/null; then \
	      echo "verify-generators: $$f COMPILED -- the include set no longer bounds generators/" >&2; exit 1; \
	    fi; \
	    if ! c++ "$$f" $(CXXSTD) $(INC_GENERATORS) -fsyntax-only 2>&1 | grep -q "file not found"; then \
	      echo "verify-generators: $$f failed for some OTHER reason than an unreachable header" >&2; exit 1; \
	    fi; done; \
	  c++ test/generators/SameRegionSamePlacement.cpp $(GEN_SRCS) src/core/ClassStructure.cpp \
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
	  f=test/world/GeneratorIsNotReachable.cpp; \
	  if c++ "$$f" $(CXXSTD) $(INC_WORLD) -fsyntax-only 2>/dev/null; then \
	    echo "verify-world: $$f COMPILED -- world/ can name a generator" >&2; exit 1; \
	  fi; \
	  if ! c++ "$$f" $(CXXSTD) $(INC_WORLD) -fsyntax-only 2>&1 | grep -q "file not found"; then \
	    echo "verify-world: $$f failed for some OTHER reason than an unreachable header" >&2; exit 1; \
	  fi; \
	  echo "verify-world: generators/ and render/ have no spelling in world/"

verify-types:    ## a value whose misuse must not compile -- and does not
	@cd $(SELF_DIR); set -e; \
	  for f in test/core/GroundSampleIsUsable.cpp test/core/WaterDepthIsUsable.cpp; do \
	    c++ "$$f" $(CXXSTD) $(CXX_WARN) -Isrc/core -fsyntax-only; done; \
	  for f in test/core/HeightIsNotReachableWithoutItsState.cpp test/core/AnswerIsNotIgnorable.cpp \
	           test/core/DepthIsNeverNegative.cpp; do \
	    if c++ "$$f" $(CXXSTD) $(CXX_WARN) -Isrc/core -fsyntax-only 2>/dev/null; then \
	      echo "verify-types: $$f COMPILED -- a number is readable without the state that gives it meaning" >&2; exit 1; \
	    fi; done; \
	  echo "verify-types: GroundSample and WaterDepth usable, their metres unreachable without the state"

# The gate that would have caught the forest living in one main() for ten rounds: an entry point may
# not build anything, and the scene-building calls exist in exactly one translation unit.
verify-clients:  ## check that the entry points are entry points and share one scene builder
	@cd $(SELF_DIR) && python3 test/clients/verify_clients.py

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

verify-walk: walk ## the client that measures links -- render/ and clients/ against native Dawn
	@cd $(SELF_DIR); set -e; \
	  if [ ! -x build/gpu_walk ]; then echo "verify-walk: the link produced no client" >&2; exit 1; fi; \
	  echo "verify-walk: build/gpu_walk $$(shasum -a 256 build/gpu_walk | cut -c1-16)"

# WHAT demo/walk-500 DRAWS, taken from the unsanitised client on this build. A binary that exits 0
# having drawn nothing satisfies an exit code and nothing else, and so does a run that walked some
# other world. Asserting these is what makes "the sanitiser was silent over 10 800 frames" a
# statement about THIS traversal.
WALK500_FRAMES := 10800
WALK500_DRAWN  := impostorStands=9565 treeTris=19130

# THE DECLARED SANITISED RUNS, and what they decide is whether the sanitiser speaks -- never whether
# the frame holds, because an instrumented frame is not the frame anyone measures. The measuring
# target names $(SAN_NATIVE) nowhere and the objects live in their own directory, so no instrumented
# object can reach a binary anyone measures.
verify-walk-asan: walk-asan ## demo/walk-500 over its whole traversal under address+undefined
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

# THE DECLARED STILL IS ONE PICTURE, and every A/B comparison in this repository is read against its
# repeat noise: a scene that answers twice makes every judgement taken on it undecidable, whatever the
# judgement was about. What is repeated is the RUN and not the frame, because what varies between runs
# is the order the tile server's answers come back in -- and no order may reach the picture.
#
# THE ORDER IS IMPOSED, NOT SAMPLED. Plain repeats are a coin toss: on a warm cache this host returned
# the same completion order six runs out of six, and the gate would have been green over a scene that
# still had two pictures in it. test/world/tile_delay.py holds each response back by a delay derived
# from the path and a seed, so one seed is one arrival order and the seeds are the experiment.
#
# NOTHING IS PINNED TO A RECORDED VALUE. The runs are compared to EACH OTHER, so the gate says
# "deterministic" and never "unchanged": a golden sha would go red on another GPU and would have to be
# rewritten by the round that legitimately moves a pixel, which is a gate that trains its reader to
# overwrite it.
STILL_SEEDS := 1 2 3 4
STILL_SPREAD_MS := 400
STILL_PORT ?= 8171
TILES_BASE ?= http://localhost:8081

verify-still: walk ## the declared still is one picture over imposed tile arrival orders
	@cd $(SELF_DIR); r=$(GATE_DIR)/still; rm -rf "$$r"; mkdir -p "$$r"; \
	  for s in $(STILL_SEEDS); do \
	    d="$$r/$$s"; mkdir -p "$$d"; \
	    python3 test/world/tile_delay.py --port $(STILL_PORT) --upstream $(TILES_BASE) --seed "$$s" \
	      --spread $(STILL_SPREAD_MS) >"$$d/proxy.log" 2>&1 & p=$$!; \
	    for w in 1 2 3 4 5 6 7 8 9 10; do grep -q "tile_delay: :" "$$d/proxy.log" && break; sleep 0.5; done; \
	    if ! grep -q "tile_delay: :$(STILL_PORT) " "$$d/proxy.log"; then \
	      kill $$p 2>/dev/null; cat "$$d/proxy.log" >&2; \
	      echo "verify-still: no delay proxy on :$(STILL_PORT) -- nothing was imposed" >&2; exit 1; fi; \
	    OUTSHINE_TILES=http://localhost:$(STILL_PORT) OUTSHINE_OUT="$$d" \
	      ./build/gpu_walk demo frame >"$$d/out.log" 2>&1; rc=$$?; \
	    kill $$p 2>/dev/null; wait $$p 2>/dev/null; \
	    if [ "$$rc" != 0 ]; then tail -40 "$$d/out.log" >&2; \
	      echo "verify-still: the run under seed $$s did not finish ($$d/out.log)" >&2; exit 1; fi; \
	    if [ ! -s "$$d/walk.png" ]; then \
	      echo "verify-still: seed $$s finished and wrote no still ($$d/walk.png)" >&2; exit 1; fi; \
	    c=$$(grep -F "INFO run terrain " "$$d/out.log" | tail -1 | sed 's/.*terrainTris/terrainTris/'); \
	    if [ -z "$$c" ]; then \
	      echo "verify-still: seed $$s wrote a still and no counters ($$d/out.log)" >&2; exit 1; fi; \
	    printf '%s  %s\n' "$$(shasum -a 256 "$$d/walk.png" | cut -c1-16)" "$$c" >>"$$r/answers"; \
	    grep -oE "INFO world buildings added=[0-9]+" "$$d/out.log" | tr '\n' ' ' >>"$$r/orders"; \
	    echo >>"$$r/orders"; \
	  done; \
	  o=$$(sort -u "$$r/orders" | wc -l | tr -d ' '); \
	  if [ "$$o" -lt 2 ]; then \
	    echo "verify-still: the delays imposed ONE ingest order over $(words $(STILL_SEEDS)) seeds -- nothing was decided ($$r/orders)" >&2; \
	    exit 1; fi; \
	  n=$$(sort -u "$$r/answers" | wc -l | tr -d ' '); \
	  if [ "$$n" != 1 ]; then \
	    sort "$$r/answers" | uniq -c >&2; \
	    echo "verify-still: $$o ingest orders drew $$n different pictures ($$r/answers)" >&2; exit 1; fi; \
	  echo "verify-still: $$o imposed ingest orders, one picture $$(head -1 "$$r/answers")"

# EVERY GATE A ROUND MUST NOT BREAK, IN ONE COMMAND. The negatives were .PHONY and nothing obliged a
# round to type them -- the failure mode of a debug configuration nobody builds. Every gate runs even
# after one has fallen, because the second failure is information the first one would have hidden.
#
# THE SPLIT IS WHAT A GATE DECIDES, NOT WHEN IT IS RUN. $(GATES_BUILD) is everything a compiler and a
# refusing program can settle: the layering, what the type system must refuse, what a running program
# must refuse, and the link. $(GATES_RUN) is what only a client that RUNS can settle -- a sanitiser
# that speaks only over a traversal, and a picture that is only one picture over repeated runs. The
# second set needs a 10 800-frame walk and $(words $(STILL_SEEDS)) whole scenes and costs two orders
# of magnitude more, so the first one exists to be affordable in an edit: a gate that is skipped is
# not a defence.
GATES_BUILD := verify-generators verify-clients verify-types verify-world verify-refusals \
  verify-walk
GATES_RUN := verify-still verify-walk-asan
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

gates-build:     ## the gates a compiler and a refusal decide -- no traversal
	$(call RUN_GATES,$(GATES_BUILD),gates-build)

clean:           ## remove build artifacts
	cd $(SELF_DIR) && rm -rf build/obj-walk build/obj-world build/obj-walk-asan \
	  build/gpu_walk build/gpu_walk_asan build/fb_world build/gen_gate build/treebench
