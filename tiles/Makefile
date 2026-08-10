# fb-tiles — the world-data service. Self-contained: this Makefile, Dockerfile, C source and the
# vendored osmmesh library all live under this one directory; nothing here reaches outside it.
#
#   make build   host-compile fb-tiles for local dev -> tiles/fb-tiles
#   make run     build + run it on TILES_PORT (default 8081), cache in ./cache
#   make image   build the deployable container (podman) -- works from any cwd, always uses the
#                repo root as build context, since tiles/osmmesh must be reachable
#   make clean   remove the local dev binary + cache
SHELL := /bin/bash
CC       ?= gcc
# -I. : osmmesh/ is flat (mvt.h etc. sit directly in it, no include/ subdir), so `<osmmesh/mvt.h>`
# resolves as ./osmmesh/mvt.h from this directory -- no extra -I needed for src/ (quoted includes
# there resolve relative to each source file, which is all same-directory siblings).
CFLAGS   ?= -O2 -Wall -Wextra -Wpedantic -Werror -I.
# Most dev machines have libcurl4-openssl-dev installed and need nothing extra. A machine with only
# the runtime .so (no curl.h) can point these at a local compat header without touching this file,
# e.g.: make build CURL_CFLAGS=-I/path/to/curl-compat/include CURL_LIBS='-L/path/to/lib -lcurl'
CURL_CFLAGS ?=
CURL_LIBS   ?= -lcurl
TILES_PORT  ?= 8081

SRC := $(addprefix src/,main.c cache.c route.c tilesrc.c prefetch.c raster.c bake.c \
       bakepool.c tilemap.c stars.c grib2.c wx.c peaks.c) $(wildcard osmmesh/*.c)
BIN := fb-tiles

# This Makefile's own directory, absolute -- so `image` resolves the repo root correctly no matter
# whether it's invoked as `make -C tiles image`, `cd tiles && make image`, or via a parent Makefile.
SELF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.PHONY: build run image clean

build: $(BIN)   ## host-compile fb-tiles for local dev -> tiles/fb-tiles

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(CURL_CFLAGS) $(SRC) -o $@ -lm -lpthread $(CURL_LIBS)

run: build      ## build + run locally (TILES_PORT, TILES_CACHE env as usual)
	TILES_PORT=$(TILES_PORT) TILES_CACHE=$${TILES_CACHE:-./cache} ./$(BIN)

image:          ## build the deployable container -- context = repo root, not tiles/
	cd $(SELF_DIR)/.. && podman build -f tiles/Dockerfile -t fb-tiles .

clean:          ## remove the local dev binary + cache
	rm -f $(BIN)
	rm -rf cache
