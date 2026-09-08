# Outshine — the library under src/, its declared data under src/assets/, the tests and the mods
# they run under test/. This Makefile is the ONE way in: nothing here is started by reaching past
# it into a script.
#
# WHAT EACH TARGET IS FOR
#
#   make strip      delete every comment src/ may not keep. `include/` and `src/client/` keep
#                   their Doxygen; the rest of src/ keeps NOTHING, and seeing that on
#                   every build is what forces code that speaks for itself
#   make            the library -> build/liboutshine.a, the generators -> build/libgenerators.a,
#                   and the tools beside them
#   make db         compile_commands.json, derived from the SAME tier graph the build uses
#   make lint       clang-format, clang-tidy and this tree's own repository rules
#   make doc        the door's documentation -> build/doc
#   make shots      every place through the camera -> build/shots  (PLACE=Wien for one)
#   make test       the fast gate
#   make suite      one named suite                                (SUITE=outshine/places)
#   make clean      remove build artefacts
#   make spotless   and the compiler's own nest in the system temp directory
#
# THE LAYERING IS THE BUILD AND IT IS DECLARED ONCE, in src/<tier>/reaches. test/run.sh derives
# every include set from it, so this file keeps NO second map -- one went stale, left three layers
# out of the archive and broke `make` at HEAD (board:1584).
SHELL := /bin/bash
.DEFAULT_GOAL := all

SELF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
RUN      := cd $(SELF_DIR) && sh test/run.sh

# clang-tidy and clang-format ship with LLVM and are NOT on the default path on this platform;
# naming the directory once here is the difference between a lint that runs and a lint that is
# quietly skipped.
LLVM_BIN := /opt/homebrew/opt/llvm/bin

.PHONY: crown-provenance all strip shader-tools shaders db lint doc shots corpus-render corpus-prepare test suite clean spotless help

GLSLANG ?= $(SELF_DIR)/build/deps/install/bin/glslangValidator
GLSL_SHADERS := $(wildcard src/render/shaders/*.comp src/render/shaders/*.vert src/render/shaders/*.frag)
SPIRV_SHADERS := $(patsubst src/render/shaders/%,build/shaders/%.spv,$(GLSL_SHADERS))

GROUND_OUTPUTS := $(foreach v,0 1,$(foreach n,0 1 2 3,$(foreach i,0 1 2 3,$(if $(and $(filter 1,$(v)),$(filter 1,$(n) $(i))),,$(if $(and $(filter-out 0,$(n)),$(filter $(n),$(i))),,$(v)$(n)$(i))))))
GROUND_SHADERS := $(foreach v,0 1,build/shaders/groundLattice-$(v).vert.spv) $(foreach output,$(GROUND_OUTPUTS),build/shaders/groundLit-$(output).frag.spv)

FLAT_VERTICES := $(foreach u,0 1 2,$(foreach t,0 1,$(foreach v,0 1,build/shaders/flat-$(u)$(t)-$(v).vert.spv)))
FLAT_FRAGMENTS := $(foreach kind,00 01 10 11 20 21 30,$(foreach output,$(GROUND_OUTPUTS),build/shaders/flat-$(kind)-$(output).frag.spv))

LIT_LAYOUTS := 000 010 100 110 200 210 101 111 201 211
LIT_VERTICES := $(foreach layout,$(LIT_LAYOUTS),$(foreach v,0 1,build/shaders/lit-$(layout)-$(v).vert.spv))
LIT_FRAGMENTS := $(foreach kind,0 1 2 3,$(foreach layout,00 10 11,$(foreach output,$(GROUND_OUTPUTS),build/shaders/lit-$(kind)$(layout)-$(output).frag.spv)))
shaders: $(SPIRV_SHADERS) $(GROUND_SHADERS) $(FLAT_VERTICES) $(FLAT_FRAGMENTS) $(LIT_VERTICES) $(LIT_FRAGMENTS)

build/shaders/lit-%.vert.spv: src/render/shaders/litVertex.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@variant=$*; $(GLSLANG) -V --target-env vulkan1.0 -S vert -DLIT_UVS=$${variant:0:1} -DLIT_TINTED=$${variant:1:1} -DLIT_MAPPED=$${variant:2:1} -DSUBJECT_WRITES_VELOCITY=$${variant:4:1} $< -o $@

build/shaders/lit-%.frag.spv: src/render/shaders/litFragment.glsl $(wildcard src/render/shaders/*.glsl) build/shaders/brdfTables.glsl
	@mkdir -p $(@D)
	@variant=$*; $(GLSLANG) -V --target-env vulkan1.0 -S frag -Ibuild/shaders -DLIT_KIND=$${variant:0:1} -DLIT_TEXTURED=$${variant:1:1} -DLIT_MAPPED=$${variant:2:1} -DSUBJECT_WRITES_VELOCITY=$${variant:4:1} -DSUBJECT_NORMAL_LOCATION=$${variant:5:1} -DSUBJECT_IDENTITY_LOCATION=$${variant:6:1} $< -o $@

build/shaders/flat-%.vert.spv: src/render/shaders/flatVertex.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@variant=$*; $(GLSLANG) -V --target-env vulkan1.0 -S vert -DFLAT_UVS=$${variant:0:1} -DFLAT_TINTED=$${variant:1:1} -DSUBJECT_WRITES_VELOCITY=$${variant:3:1} $< -o $@

build/shaders/flat-%.frag.spv: src/render/shaders/flatFragment.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@variant=$*; $(GLSLANG) -V --target-env vulkan1.0 -S frag -DFLAT_KIND=$${variant:0:1} -DFLAT_TEXTURED=$${variant:1:1} -DSUBJECT_WRITES_VELOCITY=$${variant:3:1} -DSUBJECT_NORMAL_LOCATION=$${variant:4:1} -DSUBJECT_IDENTITY_LOCATION=$${variant:5:1} $< -o $@

build/shader-tables: test/scripts/shader-tables.cpp $(wildcard src/render/stages/*.h) $(wildcard src/base/math/*.h)
	@mkdir -p $(@D)
	@$(CXX) -std=c++23 -O2 -ffp-contract=off -Wall -Wextra -Werror -Iinclude -Isrc/base -Isrc/render/stages $$(pkg-config --cflags sdl3) $< -o $@

build/shaders/brdfTables.glsl: build/shader-tables
	@mkdir -p $(@D)
	@$< > $@.tmp && mv $@.tmp $@

build/shaders/groundLattice-%.vert.spv: src/render/shaders/groundLatticeVertex.glsl $(wildcard src/render/shaders/*.glsl) src/render/stages/GroundConstants.inc
	@mkdir -p $(@D)
	@$(GLSLANG) -V --target-env vulkan1.0 -S vert -DSUBJECT_WRITES_VELOCITY=$* $< -o $@

build/shaders/groundLit-%.frag.spv: src/render/shaders/groundLit.glsl $(wildcard src/render/shaders/*.glsl) build/shaders/brdfTables.glsl
	@variant=$*; $(GLSLANG) -V --target-env vulkan1.0 -S frag -Ibuild/shaders -DSUBJECT_WRITES_VELOCITY=$${variant:0:1} -DSUBJECT_NORMAL_LOCATION=$${variant:1:1} -DSUBJECT_IDENTITY_LOCATION=$${variant:2:1} $< -o $@

shader-tools:    ## build pinned glslang and SDL_shadercross (requires SDL3, SPIRV-Cross, CMake, Ninja)
	@cd $(SELF_DIR) && python3 test/scripts/shader-tools.py

build/shaders/%.spv: src/render/shaders/% $(wildcard src/render/shaders/*.glsl) src/render/stages/MediumCore.h src/render/stages/MediumConstants.inc src/render/stages/SceneConstants.inc src/render/stages/GroundConstants.inc
	@mkdir -p $(@D)
	@$(GLSLANG) -V --target-env vulkan1.0 $< -o $@

crown-provenance: strip shaders ## fingerprint the built crown producer inputs
	@cd $(SELF_DIR) && python3 test/scripts/crown-provenance.py

all: crown-provenance ## the library, the generator archive, and the tools beside them
	@cd $(SELF_DIR) && sh test/run.sh --library

strip:           ## delete every comment src/ may not keep, and reflow what that left behind
	@cd $(SELF_DIR) && CLANG_FORMAT=$(LLVM_BIN)/clang-format python3 test/strip-comments.py

db: crown-provenance ## compile_commands.json for clangd, clang-tidy and clang-format
	@$(RUN) --compile-db

lint: db         ## format, static analysis, and this tree's own repository rules
	@cd $(SELF_DIR) && sh test/lint.sh

doc:             ## the door's documentation -> build/doc
	@cd $(SELF_DIR) && doxygen doc/Doxyfile

shots: all       ## every place through the camera -> build/shots   (PLACE=Wien for one)
	@cd $(SELF_DIR) && build/outshine-client shots $(if $(PLACE),$(PLACE),--all)

corpus-prepare: ## prepare a vendor case and its oracle (MANIFEST=test/khronos/.../manifest.json)
	@$(if $(MANIFEST),,$(error name a MANIFEST))
	@cd $(SELF_DIR) && python3 test/harness/shared/corpus/prepare.py all --manifest "$(MANIFEST)"

corpus-render: all ## compare rendered vendor cases with their oracle PNGs (CASES='TextureTransformTest')
	@cd $(SELF_DIR) && python3 test/scripts/render_corpus.py $(CASES)

test: all        ## the fast gate
	@$(RUN)

suite: all       ## one named suite   (SUITE=outshine/places)
	@$(if $(SUITE),,$(error name it: make suite SUITE=outshine/places))
	@$(RUN) $(SUITE)

clean:           ## remove build artefacts
	cd $(SELF_DIR) && rm -rf build compile_commands.json

spotless: clean  ## and the compiler's own nest in the system temp directory
	rm -rf $${TMPDIR:-/tmp}/outshine-tests.*

help:            ## this list
	@grep -hE '^[a-z-]+:.*##' $(MAKEFILE_LIST) | sed 's/:.*##/\t/' | expand -t20
