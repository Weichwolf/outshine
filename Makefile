# Outshine — the library under src/, its declared data under src/assets/, the tests and the mods they
# run under test/. This Makefile lives beside them and is the whole of the build.
#
# THREE TARGETS AND NO OTHERS (CLAUDE.md): build the engine, run the tests, clean. There is no gate
# target and no verify-* target: everything a gate decided is a test now, and a test is run by
# test/run.sh, which is the only runner. A target that ran one test under one name was a second
# runner with a second verdict, and this repository has already paid for having two.
#
#   make          compile the library entire -> build/liboutshine.a
#   make test     run every test, one process and one verdict each -> sh test/run.sh
#   make clean    remove build artifacts
#
# THE LAYERING IS THE BUILD. Each directory compiles with its own include set, so a name a layer must
# not reach has no spelling in it; the unit tests under test/unit/ mirror src/ and are compiled with
# the same sets, which is what makes every one of them a continuous proof of its layer.
SHELL := /bin/bash
.DEFAULT_GOAL := all

# This Makefile's own directory, absolute -- recipes cd here so every path is repository-relative no
# matter where make was invoked from.
SELF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

CXX_WARN  := -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
CXXSTD    := -std=c++17

# Incremental compiles WITH header dependencies. The object-building recipes below are shell loops, not
# make pattern rules, so make's own prerequisite graph never sees a header: a plain `[ obj -nt src ]`
# guard silently keeps objects built against an OLD header, and every number measured from that
# binary is a phantom. DEPFLAGS makes each compile write its full prerequisite list to <obj>.d;
# fb_uptodate re-reads it and rebuilds unless the object is newer than its source AND every header it
# actually included.
DEPFLAGS := -MMD -MP
FB_UPTODATE := fb_uptodate() { o="$$1"; f="$$2"; d="$${o%.o}.d"; if [ ! -f "$$o" ] || [ ! -f "$$d" ] || [ "$$f" -nt "$$o" ]; then return 1; fi; for p in $$(sed -e 's/^[^:]*://' -e 's/\\//g' "$$d"); do if [ ! -e "$$p" ] || [ "$$p" -nt "$$o" ]; then return 1; fi; done; return 0; };

# ONE COMPILE GROUP PER LAYER, and the group is what the layering IS: every source is compiled with
# the include set of its own directory, so an upward include has no spelling rather than a rule.
INC_CORE     := -Isrc/core -Isrc/core/io
INC_DATA     := -Isrc/core -Isrc/data
INC_SCENARIO := -Isrc/core -Isrc/scenario
INC_GENERATORS := -Isrc/core -Isrc/generators
INC_GENDRAW  := $(INC_GENERATORS) -Isrc/generators/draw
INC_WORLD    := $(INC_CORE) -Isrc/data -Isrc/world -Isrc/world/tiles
INC_GLTF     := -Isrc/core -Isrc/gltf
# THE DECLARED INTERFACE, and its whole world is core and itself (board:1442). No device type and no
# draw call has a spelling in it: it answers what a document MEANS -- which boxes, which glyphs,
# where -- and the renderer is what puts that on a surface. That is what makes a layout checkable
# with no device in scope, which is the whole of how this corpus judges it.
INC_UI       := -Isrc/core -Isrc/ui
# THE DECLARED RENDER PLAN, and its whole world is core and itself. No device type has a spelling
# in it, which is what makes a plan checkable before a device exists -- and what makes the device
# layer replaceable underneath it.
INC_PLAN     := -Isrc/core -Isrc/render/plan
# THE DRAW LIST, one level below a stage: the per-draw quantities a pass carries -- the sort
# key, the batching, the surface state. No device type has a spelling in it either, so a
# draw list is buildable and checkable with no device in scope.
INC_DRAWLIST := -Isrc/core -Isrc/render/draw
INC_RENDER   := $(INC_CORE) -Isrc/render/plan -Isrc/render/draw -Isrc/render -Isrc/render/stages
INC_SIMHALF  := $(INC_CORE) -Isrc/data -Isrc/scenario -Isrc/world -Isrc/world/tiles -Isrc/generators -Isrc/clients
INC_CLIENTS  := $(INC_SIMHALF) -Isrc/generators/draw -Isrc/gltf $(INC_RENDER)
# THE HOST SEAM'S IMPLEMENTATIONS, which a test supplies and the library never names.
INC_HOST     := -Isrc/core -Isrc/data -Itools/host

SDL_IMAGE_CFLAGS := $(shell pkg-config --cflags sdl3-image)
SDL_CFLAGS       := $(shell pkg-config --cflags sdl3)

CORE_SRCS      := $(wildcard src/core/*.cpp) $(wildcard src/core/io/*.cpp)
DATA_SRCS      := $(wildcard src/data/*.cpp)
SCENARIO_SRCS  := $(wildcard src/scenario/*.cpp)
GLTF_SRCS      := $(wildcard src/gltf/*.cpp)
UI_SRCS        := $(wildcard src/ui/*.cpp)
GEN_SRCS       := $(wildcard src/generators/*.cpp)
GEN_DRAW_SRCS  := $(wildcard src/generators/draw/*.cpp)
WORLD_SRCS     := $(wildcard src/world/*.cpp) $(wildcard src/world/tiles/*.cpp)
PLAN_SRCS      := $(wildcard src/render/plan/*.cpp)
DRAWLIST_SRCS  := $(wildcard src/render/draw/*.cpp)
RENDER_SRCS    := $(wildcard src/render/*.cpp) $(wildcard src/render/stages/*.cpp)
SIM_SRCS       := src/clients/Sim.cpp src/clients/LogSinks.cpp src/clients/StreamTelemetry.cpp \
  src/clients/EyeTelemetry.cpp src/clients/CsvTelemetry.cpp src/clients/Species.cpp \
  src/clients/RegionForge.cpp
# THE SETUP CALLS A CONSUMER MAKES over the renderer, and the picture medium a test writes with.
APP_SRCS       := src/clients/GltfStudio.cpp src/clients/Image.cpp
HOST_SRCS      := $(wildcard tools/host/*.cpp)

OBJ := build/obj

.PHONY: all test clean

all:             ## compile the library entire -> build/liboutshine.a
	@cd $(SELF_DIR); set -e; \
	  $(FB_UPTODATE) \
	  mkdir -p $(OBJ) build; \
	  CC="c++ $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS)"; \
	  CCPP="c++ -std=c++20 -O2 $(CXX_WARN) $(DEPFLAGS) $(SDL_CFLAGS)"; \
	  objs=""; \
	  build_group() { inc="$$1"; compiler="$$2"; shift 2; \
	    for f in "$$@"; do o=$(OBJ)/$$(dirname "$$f" | tr / -)-$$(basename "$$f" .cpp).o; \
	      fb_uptodate "$$o" "$$f" || eval "$$compiler \"$$f\" $$inc -c -o \"$$o\""; \
	      objs="$$objs $$o"; done; }; \
	  build_group "$(INC_CORE)" "$$CC" $(CORE_SRCS); \
	  build_group "$(INC_DATA)" "$$CC" $(DATA_SRCS); \
	  build_group "$(INC_SCENARIO)" "$$CC" $(SCENARIO_SRCS); \
	  build_group "$(INC_GLTF)" "$$CC" $(GLTF_SRCS); \
	  build_group "$(INC_UI)" "$$CC" $(UI_SRCS); \
	  build_group "$(INC_GENERATORS)" "$$CC" $(GEN_SRCS); \
	  build_group "$(INC_GENDRAW)" "$$CC" $(GEN_DRAW_SRCS); \
	  build_group "$(INC_WORLD) $(SDL_IMAGE_CFLAGS)" "$$CC" $(WORLD_SRCS); \
	  build_group "$(INC_PLAN)" "$$CC" $(PLAN_SRCS); \
	  build_group "$(INC_DRAWLIST)" "$$CC" $(DRAWLIST_SRCS); \
	  build_group "$(INC_RENDER)" "$$CCPP" $(RENDER_SRCS); \
	  build_group "$(INC_SIMHALF)" "$$CC" $(SIM_SRCS); \
	  build_group "$(INC_CLIENTS) $(SDL_IMAGE_CFLAGS)" "$$CCPP" $(APP_SRCS); \
	  build_group "$(INC_HOST)" "$$CC" $(HOST_SRCS); \
	  rm -f build/liboutshine.a; \
	  ar rcs build/liboutshine.a $$objs; \
	  echo "-> build/liboutshine.a ($$(echo $$objs | wc -w | tr -d ' ') objects)"

test:            ## run every test, one process and one verdict each
	@cd $(SELF_DIR) && sh test/run.sh

clean:           ## remove build artifacts
	cd $(SELF_DIR) && rm -rf build
