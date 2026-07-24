# fb-sim — the FlightBox Command Center. Self-contained: this Makefile, Dockerfile, the C++ source
# under src/, and the vendored toolchain under vendor/ (JSBSim submodule, Dawn, emdawnwebgpu, stb,
# build scripts) all live under this one directory. libJSBSim is compiled from the pinned
# vendor/jsbsim submodule; the only cross-directory reference is fb-tiles' public bake-version header
# (../tiles/src/style_ver.h).
#
#   make wasm     build the WASM browser Command Center (F-16 sim + renderer) -> web/gpu.js
#   make worker   build the off-thread tile worker (fetch/decode/mesh/mips)   -> web/fbtileworker.js
#   make native   build the native headless WebGPU oracle (Dawn + --fly)      -> build/gpu_native
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

DAWN_OUT     := vendor/dawn/out
DAWN_LIBDIR  := $(DAWN_OUT)/src/dawn/native
CURL_COMPAT  := vendor/.compat-headers
JSBSIM       := vendor/jsbsim
JSB_WASM     := build/jsbsim-wasm
JSB_HOST     := build/jsbsim-host

# Shared include roots (flat -I per module dir; #include stays bare-filename). core = FBState/
# FBMode/telemetry; math = value-math (FBMat4); render = FBRenderer + camera/mips/chunkmesh/ephemeris;
# world = FBWorld/FBTerrainField/tile streaming; systems = the DEFAULT, airframe-agnostic guidance/
# FCS/planner (FBAutopilot/FBFlightControl/FBPathPlan) modules compose and may override; modules(/f16
# (/displays)) = the pluggable-aircraft base + the F-16's module (composes systems/ + its gain preset)
# and HUD; fdm = the JSBSim C adapter; terrain = the lean terrain API (flat); vendor/jsbsim/src = the
# JSBSim headers.
INC_MODULES := -Isrc/core -Isrc/math -Isrc/render -Isrc/world -Isrc/systems -Isrc/modules -Isrc/modules/f16 -Isrc/modules/f16/displays
INC_WASM   := $(INC_MODULES) -Isrc/fdm -Isrc/terrain -I$(JSBSIM)/src
INC_NATIVE := $(INC_MODULES) -Isrc/fdm -Isrc/terrain
INC_TILES := -I../tiles/src

.PHONY: help wasm worker native image up down restart clean

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

wasm:            ## build the WASM browser Command Center (F-16 sim + renderer) -> web/gpu.js
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  test -f $(JSB_WASM)/src/libJSBSim.a || bash vendor/build_jsbsim_wasm.sh; \
	  mkdir -p build/obj-wasm; \
	  for f in src/terrain/*.cpp; do o=build/obj-wasm/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) -Isrc/terrain -Ivendor -c -o "$$o"; done; \
	  emcc src/world/FBTerrainLoader.cpp $(CXXSTD) -O1 $(EMCC_WARN) $(INC_MODULES) -Isrc/terrain $(INC_TILES) \
	    -c -o build/obj-wasm/FBTerrainLoader.o; \
	  emcc src/fdm/jsbsim_adapter.cpp -std=c++17 -O2 $(EMCC_WARN) -fexceptions \
	    -include src/fdm/em_compat.h -I$(JSBSIM)/src -Isrc/fdm -c -o build/obj-wasm/jsbsim_adapter.o; \
	  emcc src/systems/FBAutopilot.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_MODULES) -Isrc/terrain \
	    -c -o build/obj-wasm/FBAutopilot.o; \
	  emcc src/systems/FBFlightControl.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBFlightControl.o; \
	  emcc src/world/FBTerrainField.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_MODULES) -Isrc/terrain \
	    -c -o build/obj-wasm/FBTerrainField.o; \
	  emcc src/systems/FBPathPlan.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_MODULES) -Isrc/terrain \
	    -c -o build/obj-wasm/FBPathPlan.o; \
	  emcc src/modules/f16/FBF16Module.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16Module.o; \
	  emcc build/obj-wasm/*.o $(JSB_WASM)/src/libJSBSim.a \
	    src/app/FBAppWasm.cpp src/render/FBRenderer.cpp src/world/FBWorld.cpp \
	    src/render/stages/FBStarsStage.cpp src/render/stages/FBTileLightsStage.cpp \
	    src/render/stages/FBHudStage.cpp src/render/stages/FBUpscaleStage.cpp \
	    src/render/stages/FBTransmittanceStage.cpp src/render/stages/FBSkyViewStage.cpp \
	    src/render/stages/FBSkyStage.cpp src/render/stages/FBTilesStage.cpp \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O1 -Wall -Werror -fexceptions \
	    $(INC_WASM) \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_fb_toggle_ground,_fb_set_ground \
	    --embed-file $(JSBSIM)/aircraft/f16@/jsbsim/aircraft/f16 \
	    --embed-file $(JSBSIM)/engine/F100-PW-229.xml@/jsbsim/engine/F100-PW-229.xml \
	    --embed-file $(JSBSIM)/engine/direct.xml@/jsbsim/engine/direct.xml \
	    --embed-file web/moon.jpg@/moon.jpg \
	    -o web/gpu.js; \
	  echo "WASM -> web/gpu.js (served at /)"

worker:          ## build the off-thread tile worker (fetch/decode/mesh/mips) -> web/fbtileworker.js
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  mkdir -p build/obj-worker; \
	  for f in src/terrain/*.cpp; do o=build/obj-worker/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) -Isrc/terrain -Ivendor -c -o "$$o"; done; \
	  emcc build/obj-worker/*.o src/app/FBTileWorkerMain.cpp $(CXXSTD) -O2 $(EMCC_WARN) \
	    -Isrc/render -Isrc/terrain $(INC_TILES) \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sEXIT_RUNTIME=0 -sFETCH \
	    -sEXPORTED_FUNCTIONS=_malloc,_free,_fbtw_open,_fbtw_build,_fbtw_verts,_fbtw_nverts,_fbtw_err,_fbtw_origin,_fbtw_mips,_fbtw_mipbytes,_fbtw_ts,_fbtw_release \
	    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,HEAPF32,HEAPF64 \
	    -o web/fbtileworker.js; \
	  echo "WASM worker -> web/fbtileworker.js"

native:          ## build the native headless WebGPU oracle (Dawn + real fb-tiles terrain + in-process F-16 for --fly) -> build/gpu_native
	@cd $(SELF_DIR); set -e; \
	  test -f $(DAWN_LIBDIR)/libwebgpu_dawn.a || bash vendor/build_dawn_native.sh; \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  test -f $(JSB_HOST)/lib/libJSBSim.a || bash vendor/build_jsbsim_host.sh; \
	  mkdir -p build/obj-native; \
	  for f in src/terrain/*.cpp; do o=build/obj-native/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) -Isrc/terrain -isystem vendor -c -o "$$o"; done; \
	  c++ src/world/FBTerrainLoader.cpp $(CXXSTD) -O2 $(CXX_WARN) $(INC_NATIVE) $(INC_TILES) \
	    -isystem $(CURL_COMPAT) -c -o build/obj-native/FBTerrainLoader.o; \
	  c++ src/fdm/jsbsim_adapter.cpp -std=c++17 -O2 -Wall \
	    -Isrc/fdm -isystem $(JSB_HOST)/include/JSBSim -c -o build/obj-native/jsbsim_adapter.o; \
	  c++ src/systems/FBAutopilot.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_NATIVE) \
	    -c -o build/obj-native/FBAutopilot.o; \
	  c++ src/systems/FBFlightControl.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_NATIVE) \
	    -c -o build/obj-native/FBFlightControl.o; \
	  c++ src/world/FBTerrainField.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_NATIVE) \
	    -c -o build/obj-native/FBTerrainField.o; \
	  c++ src/systems/FBPathPlan.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_NATIVE) \
	    -c -o build/obj-native/FBPathPlan.o; \
	  c++ src/modules/f16/FBF16Module.cpp -std=c++17 -O2 -Wall -Werror -Isrc/fdm $(INC_NATIVE) \
	    -c -o build/obj-native/FBF16Module.o; \
	  c++ src/app/FBAppNative.cpp src/render/FBRenderer.cpp src/world/FBWorld.cpp \
	    src/render/stages/FBStarsStage.cpp src/render/stages/FBTileLightsStage.cpp \
	    src/render/stages/FBHudStage.cpp src/render/stages/FBUpscaleStage.cpp \
	    src/render/stages/FBTransmittanceStage.cpp src/render/stages/FBSkyViewStage.cpp \
	    src/render/stages/FBSkyStage.cpp src/render/stages/FBTilesStage.cpp \
	    build/obj-native/*.o $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++20 -O2 -Wall -Wextra -Werror \
	    $(INC_NATIVE) \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor \
	    -isystem $(CURL_COMPAT) -isystem $(JSB_HOST)/include/JSBSim \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm \
	    -o build/gpu_native; \
	  echo "native -> build/gpu_native"

image:           ## build the deployable fb-sim web container (context = sim/, trimmed by .containerignore)
	cd $(SELF_DIR) && podman build -f Dockerfile -t fb-sim .

up:              ## run the web host container (fb-sim, :8080; sim runs IN the browser)
	@cd $(SELF_DIR) && bash up.sh

down:            ## stop the web host
	-podman rm -f fb-sim 2>/dev/null

restart: down up ## stop and start the web host

clean:           ## remove build artifacts (WASM + worker + native)
	cd $(SELF_DIR) && rm -rf build/obj-wasm build/obj-worker build/obj-native build/gpu_native \
	  web/gpu.js web/gpu.wasm web/fbtileworker.js web/fbtileworker.wasm
