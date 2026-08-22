# Outshine — the library under src/, its declared data under src/assets/, the tests and the mods they
# run under test/. This Makefile lives beside them and is the whole of the build.
#
# THREE TARGETS AND NO OTHERS (CLAUDE.md): build the engine, run the tests, clean.
#
#   make          compile the library entire -> build/liboutshine.a
#   make test     run every test, one process and one verdict each -> sh test/run.sh
#   make clean    remove build artifacts
#
# THE LAYERING IS THE BUILD, AND IT IS DECLARED ONCE. test/run.sh's GroupIncludes is the only
# spelling of which source compiles with which includes; `make` delegates to `run.sh --library`,
# which builds every group from those same declarations and archives them. A second include map
# here went stale, left three layers out of the archive, and broke `make` at HEAD (board:1584).
SHELL := /bin/bash
.DEFAULT_GOAL := all

SELF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.PHONY: all test clean

all:             ## compile the library entire -> build/liboutshine.a
	@cd $(SELF_DIR) && sh test/run.sh --library

test:            ## run every test, one process and one verdict each
	@cd $(SELF_DIR) && sh test/run.sh

clean:           ## remove build artifacts
	cd $(SELF_DIR) && rm -rf build
