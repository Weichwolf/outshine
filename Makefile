SHELL := /bin/sh
.DEFAULT_GOAL := all
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(ROOT)/build
LLVM_BIN ?= /opt/homebrew/opt/llvm/bin
CLANG_FORMAT ?= $(LLVM_BIN)/clang-format
GLSLANG ?= $(ROOT)/build/deps/install/bin/glslang
CMAKE_ARGS ?=
CMAKE_BUILD := cmake --build "$(BUILD_DIR)" --parallel
.PHONY: all configure db shaders strip format lint lint-all lint-docs test test-all suite clean spotless help

configure:
	@mkdir -p "$(BUILD_DIR)/.cmake/api/v1/query"
	@test -f "$(BUILD_DIR)/.cmake/api/v1/query/codemodel-v2" || touch "$(BUILD_DIR)/.cmake/api/v1/query/codemodel-v2"
	@test -f "$(BUILD_DIR)/CMakeCache.txt" || cmake -S "$(ROOT)" -B "$(BUILD_DIR)" -G Ninja -DBUILD_TESTING=ON $(CMAKE_ARGS)

all: configure strip ## build the application through CMake/Ninja using all cores
	@$(CMAKE_BUILD) --target outshine

shaders: configure ## build the native shader package
	@$(CMAKE_BUILD) --target outshine_shaders

db: configure ## generate the CMake compilation database
	@cmake -S "$(ROOT)" -B "$(BUILD_DIR)" $(CMAKE_ARGS)
	@cmake -E rm -f "$(ROOT)/compile_commands.json"
	@cmake -E create_symlink "$(BUILD_DIR)/compile_commands.json" "$(ROOT)/compile_commands.json"

strip: ## strip implementation comments; preserve headers and tests
	@cd "$(ROOT)" && CLANG_FORMAT="$(CLANG_FORMAT)" python3 test/strip-comments.py

format: strip ## format owned C++ files with clang-format
	@cd "$(ROOT)" && python3 test/scripts/format_sources.py --tool "$(CLANG_FORMAT)"

lint lint-all: ## clang-format and CMake-integrated clang-tidy
	@cd "$(ROOT)" && python3 test/scripts/format_sources.py --tool "$(CLANG_FORMAT)" --check
	@cmake -S "$(ROOT)" -B "$(BUILD_DIR)/lint" -G Ninja -DBUILD_TESTING=OFF -DOUTSHINE_CLANG_TIDY=ON $(CMAKE_ARGS)
	@cmake --build "$(BUILD_DIR)/lint" --target outshine --parallel $(if $(filter lint-all,$@),--clean-first)

lint-docs: ## whitespace check for changed documentation
	@cd "$(ROOT)" && git diff --check -- '*.md'

test: configure ## execute the world gate through CTest
	@cd "$(ROOT)" && OUTSHINE_BUILD_DIR="$(BUILD_DIR)" sh test/run.sh

test-all: configure ## build and run every registered CTest
	@cd "$(ROOT)" && OUTSHINE_BUILD_DIR="$(BUILD_DIR)" sh test/run.sh --all

suite: configure ## build and run a selected test or suite (SUITE=...)
	@test -n "$(SUITE)" || { echo 'set SUITE to a test path'; exit 2; }
	@cd "$(ROOT)" && OUTSHINE_BUILD_DIR="$(BUILD_DIR)" sh test/run.sh $(SUITE)

test-sanitizers test-races: configure ## run selected tests under separate sanitizer builds
	@test -n "$(SUITE)" || { echo 'set SUITE to a test path'; exit 2; }
	@cd "$(ROOT)" && OUTSHINE_BUILD_DIR="$(BUILD_DIR)" sh test/run.sh --sanitizer $(if $(filter test-races,$@),thread,address) --harden $(SUITE)

clean: ## clean declared CMake build products
	@$(CMAKE_BUILD) --target clean

spotless: clean

shader-tools: ## prepare the pinned shader compiler
	@cd "$(ROOT)" && python3 test/scripts/shader-tools.py

cleanup-temp: ## preview obsolete owned temp artifacts (CLEANUP_ARGS=--apply)
	@cd "$(ROOT)" && python3 test/scripts/cleanup_temp.py $(CLEANUP_ARGS)

test-cleanup-temp: ## check owned temporary artifact cleanup
	@cd "$(ROOT)" && python3 -B test/scripts/test_cleanup_temp.py

shots: all ## run the external Places driver (PLACE=...)
	@cd "$(ROOT)" && "$(BUILD_DIR)/outshine" shots $(if $(PLACE),$(PLACE),--all)

prepare-place: all ## prepare an external Place scene (PLACE=...)
	@test -n "$(PLACE)" || { echo 'set PLACE'; exit 2; }
	@cd "$(ROOT)" && "$(BUILD_DIR)/outshine" prepare $(PLACE) $(PREPARE_SECONDS)

corpus-prepare corpus-fetch: ## prepare or fetch pinned comparison inputs (MANIFEST=...)
	@test -n "$(MANIFEST)" || { echo 'set MANIFEST'; exit 2; }
	@cd "$(ROOT)" && python3 test/harness/shared/corpus/prepare.py $(if $(filter corpus-fetch,$@),fetch,all) --manifest "$(MANIFEST)"

help: ## show available targets
	@awk 'BEGIN {FS = ":.*## "} /^[a-zA-Z_ -]+:.*## / {printf "%-24s %s\n", $$1, $$2}' "$(ROOT)/Makefile"
