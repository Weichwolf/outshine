# fb-tiles — the world-data service. This Makefile, the Dockerfile and the C++ source live here; the
# DECODERS do not, and that is the point: one terrarium decoder and one MVT reader exist in the
# library and both sides use those.
#
#   make build   host-compile fb-tiles for local dev -> tiles/fb-tiles
#   make run     build + run it on TILES_PORT (default 8081), cache in ./cache
#   make image   build the deployable container (podman) -- works from any cwd, always uses the
#                repo root as build context, since the library it links lives in src/
#   make clean   remove the local dev binary + cache
SHELL := /bin/bash
# C++, and there is no C left: the decoders this server used to carry its own copies of live in the
# library now (src/world/tiles, src/world/OsmVector), so this links against THEM and cannot drift
# from them.
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -I. -I../src/world -I../src/core
SDL_IMAGE_CFLAGS := $(shell pkg-config --cflags sdl3-image)
SDL_IMAGE_LIBS   := $(shell pkg-config --libs sdl3-image)
# Most dev machines have libcurl4-openssl-dev installed and need nothing extra. A machine with only
# the runtime .so (no curl.h) can point these at a local compat header without touching this file,
# e.g.: make build CURL_CFLAGS=-I/path/to/curl-compat/include CURL_LIBS='-L/path/to/lib -lcurl'
CURL_CFLAGS ?=
CURL_LIBS   ?= -lcurl
TILES_PORT  ?= 8081

SRC := $(addprefix src/,main.cpp cache.cpp route.cpp tilesrc.cpp prefetch.cpp raster.cpp bake.cpp \
       bakepool.cpp tilemap.cpp stars.cpp grib2.cpp wx.cpp peaks.cpp) \
       ../src/world/OsmVector.cpp
BIN := fb-tiles

# This Makefile's own directory, absolute -- so `image` resolves the repo root correctly no matter
# whether it's invoked as `make -C tiles image`, `cd tiles && make image`, or via a parent Makefile.
SELF_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.PHONY: build run image clean

build: $(BIN)   ## host-compile fb-tiles for local dev -> tiles/fb-tiles

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SDL_IMAGE_CFLAGS) $(CURL_CFLAGS) $(SRC) -o $@ -lm -lpthread $(CURL_LIBS) $(SDL_IMAGE_LIBS)

run: build      ## build + run locally (TILES_PORT, TILES_CACHE env as usual)
	TILES_PORT=$(TILES_PORT) TILES_CACHE=$${TILES_CACHE:-./cache} ./$(BIN)

image:          ## build the deployable container -- context = repo root, not tiles/
	cd $(SELF_DIR)/.. && podman build -f tiles/Dockerfile -t fb-tiles .

clean:          ## remove the local dev binary + cache
	rm -f $(BIN)
	rm -rf cache
