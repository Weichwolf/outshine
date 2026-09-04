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
#   make shots      every place through the camera -> build/shots  (PLACE=Shibuya for one)
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

.PHONY: all strip db lint doc shots test suite clean spotless help

all: strip       ## the library, the generator archive, and the tools beside them
	@cd $(SELF_DIR) && sh test/run.sh --library

strip:           ## delete every comment src/ may not keep, and reflow what that left behind
	@cd $(SELF_DIR) && CLANG_FORMAT=$(LLVM_BIN)/clang-format python3 test/strip-comments.py

db:              ## compile_commands.json for clangd, clang-tidy and clang-format
	@$(RUN) --compile-db

lint: db         ## format, static analysis, and this tree's own repository rules
	@cd $(SELF_DIR) && sh test/lint.sh

doc:             ## the door's documentation -> build/doc
	@cd $(SELF_DIR) && doxygen doc/Doxyfile

shots: all       ## every place through the camera -> build/shots   (PLACE=Shibuya for one)
	@cd $(SELF_DIR) && build/outshine-client shots $(if $(PLACE),$(PLACE),--all)

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
