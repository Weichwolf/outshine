# Outshine — the library under src/, its declared data under src/assets/, the tests and the mods
# they run under test/. This Makefile is the ONE way in: nothing here is started by reaching past
# it into a script.
#
# WHAT EACH TARGET IS FOR
#
#   make strip      delete every comment src/ may not keep. `include/` keeps
#                   Doxygen; src/ keeps NOTHING, test/ keeps all comments, and seeing that on
#                   every build is what forces code that speaks for itself
#   make            the library -> build/liboutshine.a, the generators -> build/libgenerators.a,
#                   and the tools beside them
#   make db         compile_commands.json, derived from the SAME tier graph the build uses
#   make lint       clang-format, clang-tidy and this tree's own repository rules
#   make shots      every place through the camera -> build/shots  (PLACE=Wien for one)
#   make test       the world gate; compatibility corpora run explicitly
#   make suite      one named suite                                (SUITE=outshine/integration/places)
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

.PHONY: all strip shader-tools shaders db lint shots corpus-prepare test suite clean spotless help

GLSLANG ?= $(SELF_DIR)/build/deps/install/bin/glslangValidator
GLSLANG_FLAGS := -V --target-env vulkan1.0 -Isrc -Isrc/content/shade
GLSL_SHADERS := $(wildcard src/render/shaders/*.comp src/render/shaders/*.vert src/render/shaders/*.frag)
SPIRV_SHADERS := $(patsubst src/render/shaders/%,build/shaders/%.spv,$(GLSL_SHADERS))

GROUND_OUTPUTS := $(foreach v,0 1,$(foreach n,0 1 2 3,$(foreach i,0 1 2 3,$(if $(and $(filter 1,$(v)),$(filter 1,$(n) $(i))),,$(if $(and $(filter-out 0,$(n)),$(filter $(n),$(i))),,$(v)$(n)$(i))))))
GROUND_OUTPUTS += 01234
GROUND_SHADERS := $(foreach v,0 1,build/shaders/groundLattice-$(v).vert.spv) $(foreach output,$(GROUND_OUTPUTS),build/shaders/groundLit-$(output).frag.spv)

FLAT_VERTICES := $(foreach u,0 1 2,$(foreach t,0 1,$(foreach v,0 1,build/shaders/flat-$(u)$(t)-$(v).vert.spv)))
FLAT_FRAGMENTS := $(foreach kind,00 01 10 11 20 21 30,$(foreach output,$(GROUND_OUTPUTS),build/shaders/flat-$(kind)-$(output).frag.spv))

LIT_LAYOUTS := 000 010 100 110 200 210 101 111 201 211
LIT_VERTICES := $(foreach layout,$(LIT_LAYOUTS),$(foreach v,0 1,build/shaders/lit-$(layout)-$(v).vert.spv))
LIT_FRAGMENTS := $(foreach kind,0 1 2 3,$(foreach layout,00 10 11,$(foreach output,$(GROUND_OUTPUTS),build/shaders/lit-$(kind)$(layout)-$(output).frag.spv)))
SHADOW_MASK_SHADERS := $(foreach u,0 1 2,$(foreach c,0 1,build/shaders/subjectShadow-$(u)$(c).vert.spv)) $(foreach u,0 1 2,build/shaders/subjectShadow-$(u).frag.spv)
SHADER_ARTIFACTS := $(sort $(SPIRV_SHADERS) $(GROUND_SHADERS) $(FLAT_VERTICES) $(FLAT_FRAGMENTS) $(LIT_VERTICES) $(LIT_FRAGMENTS) $(SHADOW_MASK_SHADERS))

build/shaders/subjectShadow-%.vert.spv: src/render/shaders/subjectShadowVertex.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@stem='$*'; $(GLSLANG) $(GLSLANG_FLAGS) -S vert -DSHADOW_UVS=$${stem%?} -DSHADOW_TINTED=$${stem#?} $< -o $@

build/shaders/subjectShadow-%.frag.spv: src/render/shaders/subjectShadowFragment.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@$(GLSLANG) $(GLSLANG_FLAGS) -S frag -DSHADOW_UVS=$* $< -o $@
shaders: $(SHADER_ARTIFACTS) build/compute-shaders.json
	@mkdir -p build
	@printf '%s\n' $(SHADER_ARTIFACTS) > build/shader-artifacts.txt.tmp
	@mv build/shader-artifacts.txt.tmp build/shader-artifacts.txt

build/compute-shader-contracts: test/scripts/compute-shader-contracts.cpp src/render/stages/ComputeShaders.h src/render/stages/KernelShape.h
	@mkdir -p $(@D)
	@$(CXX) -std=c++23 -O2 -Wall -Wextra -Werror -Isrc/render/stages $< -o $@

build/compute-shaders.json: build/compute-shader-contracts
	@$< > $@.tmp && mv $@.tmp $@

build/shaders/lit-%.vert.spv: src/render/shaders/litVertex.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@variant=$*; $(GLSLANG) $(GLSLANG_FLAGS) -S vert -DLIT_UVS=$${variant:0:1} -DLIT_TINTED=$${variant:1:1} -DLIT_MAPPED=$${variant:2:1} -DSUBJECT_WRITES_VELOCITY=$${variant:4:1} $< -o $@

build/shaders/lit-%.frag.spv: src/render/shaders/litFragment.glsl $(wildcard src/render/shaders/*.glsl) build/shaders/brdfTables.glsl
	@mkdir -p $(@D)
	@variant=$*; base=$${variant:7:1}; metal=$${variant:8:1}; $(GLSLANG) $(GLSLANG_FLAGS) -S frag -Ibuild/shaders -DLIT_KIND=$${variant:0:1} -DLIT_TEXTURED=$${variant:1:1} -DLIT_MAPPED=$${variant:2:1} -DSUBJECT_WRITES_VELOCITY=$${variant:4:1} -DSUBJECT_NORMAL_LOCATION=$${variant:5:1} -DSUBJECT_IDENTITY_LOCATION=$${variant:6:1} -DSUBJECT_BASE_LOCATION=$${base:-0} -DSUBJECT_METALROUGH_LOCATION=$${metal:-0} $< -o $@

build/shaders/flat-%.vert.spv: src/render/shaders/flatVertex.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@variant=$*; $(GLSLANG) $(GLSLANG_FLAGS) -S vert -DFLAT_UVS=$${variant:0:1} -DFLAT_TINTED=$${variant:1:1} -DSUBJECT_WRITES_VELOCITY=$${variant:3:1} $< -o $@

build/shaders/flat-%.frag.spv: src/render/shaders/flatFragment.glsl $(wildcard src/render/shaders/*.glsl)
	@mkdir -p $(@D)
	@variant=$*; base=$${variant:6:1}; metal=$${variant:7:1}; $(GLSLANG) $(GLSLANG_FLAGS) -S frag -DFLAT_KIND=$${variant:0:1} -DFLAT_TEXTURED=$${variant:1:1} -DSUBJECT_WRITES_VELOCITY=$${variant:3:1} -DSUBJECT_NORMAL_LOCATION=$${variant:4:1} -DSUBJECT_IDENTITY_LOCATION=$${variant:5:1} -DSUBJECT_BASE_LOCATION=$${base:-0} -DSUBJECT_METALROUGH_LOCATION=$${metal:-0} $< -o $@

build/shader-tables: test/scripts/shader-tables.cpp $(wildcard src/render/stages/*.h) $(wildcard src/base/math/*.h)
	@mkdir -p $(@D)
	@$(CXX) -std=c++23 -O2 -ffp-contract=off -Wall -Wextra -Werror -Iinclude -Isrc/base -Isrc/render/stages $$(pkg-config --cflags sdl3) $< -o $@

build/shaders/brdfTables.glsl: build/shader-tables
	@mkdir -p $(@D)
	@$< > $@.tmp && mv $@.tmp $@

build/shaders/groundLattice-%.vert.spv: src/render/shaders/groundLatticeVertex.glsl $(wildcard src/render/shaders/*.glsl) src/render/stages/GroundConstants.inc
	@mkdir -p $(@D)
	@$(GLSLANG) $(GLSLANG_FLAGS) -S vert -DSUBJECT_WRITES_VELOCITY=$* $< -o $@

build/shaders/groundLit-%.frag.spv: src/render/shaders/groundLit.glsl $(wildcard src/render/shaders/*.glsl) build/shaders/brdfTables.glsl
	@variant=$*; base=$${variant:3:1}; metal=$${variant:4:1}; $(GLSLANG) $(GLSLANG_FLAGS) -S frag -Ibuild/shaders -DSUBJECT_WRITES_VELOCITY=$${variant:0:1} -DSUBJECT_NORMAL_LOCATION=$${variant:1:1} -DSUBJECT_IDENTITY_LOCATION=$${variant:2:1} -DSUBJECT_BASE_LOCATION=$${base:-0} -DSUBJECT_METALROUGH_LOCATION=$${metal:-0} $< -o $@

shader-tools:    ## build pinned glslang and SDL_shadercross (requires SDL3, SPIRV-Cross, CMake, Ninja)
	@cd $(SELF_DIR) && python3 test/scripts/shader-tools.py

build/shaders/%.spv: src/render/shaders/% $(wildcard src/render/shaders/*.glsl) src/world/sky/AtmosphereCore.h src/world/sky/AtmosphereConstants.inc src/render/stages/SceneConstants.inc src/render/stages/GroundConstants.inc
	@mkdir -p $(@D)
	@$(GLSLANG) $(GLSLANG_FLAGS) $< -o $@

all: strip shaders ## the library, the generator archive, and the tools beside them
	@cd $(SELF_DIR) && sh test/run.sh --library

test-strip-comments: ## verify comment policy and lexical preservation
	@cd $(SELF_DIR) && python3 test/scripts/test_strip_comments.py

strip: test-strip-comments ## remove src comments, keep only include Doxygen; leave test comments intact
	@cd $(SELF_DIR) && CLANG_FORMAT=$(LLVM_BIN)/clang-format python3 test/strip-comments.py

db: strip shaders ## compile_commands.json for clangd, clang-tidy and clang-format
	@$(RUN) --compile-db

.PHONY: experiment-tree-cost
experiment-tree-cost: all db ## compare native tree expansion/instancing (CPU, no GPU proof)
	@cd $(SELF_DIR) && python3 test/experiments/render/tree_representation_cost.py

.PHONY: experiment-metal
experiment-metal: all ## direct Metal MRT/depth/Hi-Z proof; METAL_OUTPUT must name a system-temp directory
	@test -n "$(METAL_OUTPUT)" || { echo 'set METAL_OUTPUT to a system-temp directory' >&2; exit 2; }
	@cd $(SELF_DIR) && CXX="$(CXX)" GLSLANG="$(GLSLANG)" python3 test/experiments/render/native_metal.py --output "$(METAL_OUTPUT)"

.PHONY: experiment-vulkan
experiment-vulkan: all ## native Vulkan archive/compute/textures; VULKAN_OUTPUT must name a system-temp directory
	@test -n "$(VULKAN_OUTPUT)" || { echo 'set VULKAN_OUTPUT to a system-temp directory' >&2; exit 2; }
	@cd $(SELF_DIR) && CXX="$(CXX)" GLSLANG="$(GLSLANG)" VULKAN_VALIDATION="$(VULKAN_VALIDATION)" python3 test/experiments/render/native_vulkan.py --output "$(VULKAN_OUTPUT)"

lint: ## format, static analysis, and this tree's own repository rules
	@cd $(SELF_DIR) && GLSLANG="$(GLSLANG)" sh test/lint.sh

.PHONY: lint-docs
lint-docs: ## AGENTS contracts only; offline, no GitHub or engine build
	@cd $(SELF_DIR) && sh test/lint-docs.sh

.PHONY: test-sanitizers test-races
test-sanitizers: ## native ASan/UBSan and container checks (SUITE=...)
	@$(if $(SUITE),,$(error name a SUITE))
	@$(RUN) --sanitizer address --harden $(SUITE)

test-races: ## separate native ThreadSanitizer and container checks (SUITE=...)
	@$(if $(SUITE),,$(error name a SUITE))
	@$(RUN) --sanitizer thread --harden $(SUITE)

PREPARE_SECONDS ?= 1800
prepare-place: all ## prepare generated assets and terrain without a frame-rate claim (PLACE=Koerbersee)
	@$(if $(PLACE),,$(error name a PLACE))
	@cd $(SELF_DIR) && build/outshine-client prepare $(PLACE) $(PREPARE_SECONDS)

shots: all       ## every place through the camera -> build/shots   (PLACE=Wien for one)
	@cd $(SELF_DIR) && build/outshine-client shots $(if $(PLACE),$(PLACE),--all)

corpus-prepare: ## prepare pinned corpus inputs (MANIFEST=test/.../manifest.json)
	@$(if $(MANIFEST),,$(error name a MANIFEST))
	@cd $(SELF_DIR) && python3 test/harness/shared/corpus/prepare.py all --manifest "$(MANIFEST)"

test: test-timeout-cleanup test-client-arguments test-client-data test-motion-trace all ## world contracts and offline Wien/Tokyo/Central Park renders
	@$(RUN)

.PHONY: test-all
test-all: test-timeout-cleanup test-client-arguments test-client-data test-motion-trace all ## all declared native, UI and script suites
	@$(RUN) --all

suite:           ## named suite or C++ case; builds its component closure (SUITE=...)
	@$(if $(SUITE),,$(error name it: make suite SUITE=outshine/integration/places))
	@$(RUN) $(SUITE)

clean:           ## remove build artefacts
	cd $(SELF_DIR) && rm -rf build compile_commands.json

spotless: clean  ## and the compiler's own nest in the system temp directory
	rm -rf $${TMPDIR:-/tmp}/outshine-tests.*

.PHONY: cleanup-temp test-cleanup-temp
cleanup-temp: ## preview obsolete owned temp artifacts (CLEANUP_ARGS=--apply to remove)
	@cd $(SELF_DIR) && python3 test/scripts/cleanup_temp.py $(CLEANUP_ARGS)

test-cleanup-temp: ## verify cleanup ownership, active work and reference preservation
	@cd $(SELF_DIR) && python3 test/scripts/test_cleanup_temp.py

help:            ## this list
	@grep -hE '^[a-z-]+:.*##' $(MAKEFILE_LIST) | sed 's/:.*##/\t/' | expand -t20

.PHONY: test-strip-comments test-client-data
test-client-data: all ## verify external place scenarios through the client
	@cd $(SELF_DIR) && python3 test/scripts/test_place_catalog.py

.PHONY: test-tidy-analysis
test-tidy-analysis: db ## prove clean analysis and incomplete-run detection with real clang-tidy
	@cd $(SELF_DIR) && LLVM_BIN=$(LLVM_BIN) python3 test/scripts/test_tidy_analysis.py

.PHONY: format
format: strip ## apply repository formatting to the same owned files lint checks
	@cd $(SELF_DIR) && python3 test/scripts/format_sources.py --tool $(LLVM_BIN)/clang-format

.PHONY: test-format
test-format: ## verify formatting coverage, real diagnostics and failure handling
	@cd $(SELF_DIR) && LLVM_BIN=$(LLVM_BIN) python3 test/scripts/test_format_sources.py

.PHONY: test-source-units
test-source-units: ## verify native platform and Objective-C++ source selection
	@cd $(SELF_DIR) && python3 test/scripts/test_source_units.py

.PHONY: test-client-arguments
test-client-arguments: db all ## reject invalid client coordinates before platform or provider startup
	@cd $(SELF_DIR) && python3 test/scripts/test_client_arguments.py

.PHONY: test-shader-artifacts
test-shader-artifacts: shaders ## verify the shader package, reflection and negative controls
	@cd $(SELF_DIR) && GLSLANG=$(GLSLANG) python3 test/scripts/test_shader_artifacts.py
	@cd $(SELF_DIR) && python3 test/scripts/shader_artifacts.py

.PHONY: corpus-fetch
corpus-fetch: ## fetch pinned corpus inputs (MANIFEST=test/.../manifest.json)
	@$(if $(MANIFEST),,$(error name a MANIFEST))
	@cd $(SELF_DIR) && python3 test/harness/shared/corpus/prepare.py fetch --manifest "$(MANIFEST)"

.PHONY: test-writer-inventory test-motion-trace
test-motion-trace: ## verify route-camera trace continuity and lap closure with analytic controls
	@cd $(SELF_DIR) && python3 test/scripts/test_motion_trace.py

test-writer-inventory: ## verify scenario writer source inventory
	@cd $(SELF_DIR) && python3 test/scripts/test_grammar_vs_writer.py

.PHONY: test-timeout-cleanup
test-timeout-cleanup: ## verify owned process-group cleanup and foreign-process isolation
	@cd $(SELF_DIR) && python3 test/scripts/test_timeout_cleanup.py

.PHONY: test-file-stamps
test-file-stamps: ## verify portable native test file metadata and failure handling
	@cd $(SELF_DIR) && python3 test/scripts/test_file_stamps.py
