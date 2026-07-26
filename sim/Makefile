# fb-sim — the FlightBox Command Center. Self-contained: this Makefile, Dockerfile, the C++ source
# under src/, and the vendored toolchain under vendor/ (JSBSim submodule, Dawn, emdawnwebgpu, stb,
# build scripts) all live under this one directory. libJSBSim is compiled from the pinned
# vendor/jsbsim submodule; the only cross-directory reference is fb-tiles' public bake-version header
# (../tiles/src/style_ver.h).
#
#   make wasm     build the WASM Command Center (F-16 sim + renderer) + tile worker (depends on worker,
#                 so a browser boot never hangs on a missing worker) -> web/gpu.js + web/fbtileworker.js
#   make worker   build the off-thread tile worker (fetch/decode/mesh/mips) alone -> web/fbtileworker.js
#   make core-lib build the FlightBox core library (sim+missions+FDM, no render/world) -> build/libfbcore.a
#   make native   build the native headless WebGPU oracle (Dawn + --mission)  -> build/gpu_native
#   make gym      build the headless mission client (no GPU/render link)     -> build/fb-gym
#   make test-monitor  build FBFlightMonitor's gear-force negative-proof harness -> build/fb-test-hard-landing
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
# FBMode/telemetry/FBFlightPlan/FBRunway; math = value-math (FBMat4); render = FBRenderer + camera/mips/
# chunkmesh/ephemeris/the HUD's WebGPU-backend resources (FBHudGeometry, FBHudFont ROM); world = FBWorld/
# FBTerrainField/tile streaming; units = FBUnit/FBOwnshipUnit (world-entity identity/pose); systems = the
# DEFAULT, airframe-agnostic guidance/FCS/displays/pilot/airframe-controls (FBAutopilot/
# FBFlightControl/FBDisplaySystem/FBPilot/FBAirframeControls) modules compose and may
# override; modules/f16 = the F-16's module (composes systems/ + its gain preset); fdm = the JSBSim
# adapter (namespace FlightBox, ONE-TU JSBSim seam — see fdm/jsbsim_adapter.h); terrain = the lean
# terrain API (flat); vendor/jsbsim/src = the JSBSim headers.
INC_MODULES := -Isrc/core -Isrc/math -Isrc/render -Isrc/world -Isrc/units -Isrc/systems -Isrc/modules -Isrc/modules/f16 -Isrc/modules/f16/displays
INC_WASM   := $(INC_MODULES) -Isrc/fdm -Isrc/terrain -I$(JSBSIM)/src
INC_NATIVE := $(INC_MODULES) -Isrc/fdm -Isrc/terrain
INC_TILES := -I../tiles/src

# The FlightBox core library (Auftrag "Lib/Client-Split"): core/ + fdm/ + systems/ + modules/f16/ +
# units/ + libJSBSim, EVERYTHING a headless mission run needs and NOTHING render/world-streaming-
# specific (CLAUDE.md: "render/ und world/-Streaming gehoeren NICHT zur Core-Lib"). ONE exception:
# render/FBHudGeometry.cpp — physically under render/ but pure CPU vertex-list math (no WebGPU/Dawn
# include, verified), the one render/ file systems/FBDisplaySystem.cpp structurally depends on to build
# the HUD's vertex buffers; core-lib links it so fb-gym can, without pulling in any Dawn symbol (see the
# gym target's own nm check). This library is a NATIVE-TOOLCHAIN archive (build/libfbcore.a) shared by
# native + gym; the WASM client is a different toolchain entirely (emcc/wasm32) and necessarily keeps
# compiling the SAME source list itself in the wasm target below — cross-compilation, not duplication.
CORE_LIB_SRCS := \
  src/fdm/jsbsim_adapter.cpp src/fdm/FBFdmTelemetrySource.cpp \
  src/core/FBLog.cpp src/core/FBTelemetry.cpp src/core/FBMissionFile.cpp \
  src/core/FBRunwayPlateauElevation.cpp src/core/FBBakedDemElevation.cpp src/core/FBFlightMonitor.cpp \
  src/systems/FBAutopilot.cpp src/systems/FBFlightControl.cpp src/systems/FBDisplaySystem.cpp \
  src/systems/FBAirDataSystem.cpp src/systems/FBRadarAltimeter.cpp src/systems/FBNavSystem.cpp \
  src/systems/FBAirframeControls.cpp src/systems/FBPilot.cpp \
  src/modules/FBModuleRegistry.cpp \
  src/modules/f16/FBF16Max7456.cpp src/modules/f16/FBF16Module.cpp src/modules/f16/FBF16FireControl.cpp \
  src/modules/f16/FBF16Ufc.cpp src/modules/f16/FBF16Sms.cpp src/modules/f16/FBF16Pilot.cpp \
  src/modules/f16/FBF16ModuleRegistration.cpp \
  src/modules/f16/displays/FBF16Hud.cpp \
  src/units/FBOwnshipUnit.cpp \
  src/render/FBHudGeometry.cpp

.PHONY: help wasm worker core-lib native gym image up down restart clean

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

wasm: worker     ## build the WASM Command Center + tile worker -> web/gpu.{js,wasm} + web/fbtileworker.{js,wasm}
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  test -f $(JSB_WASM)/src/libJSBSim.a || bash vendor/build_jsbsim_wasm.sh; \
	  mkdir -p build/obj-wasm; \
	  for f in src/terrain/*.cpp; do o=build/obj-wasm/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) -Isrc/terrain -Isrc/core -Ivendor -c -o "$$o"; done; \
	  emcc src/world/FBTerrainLoader.cpp $(CXXSTD) -O1 $(EMCC_WARN) $(INC_MODULES) -Isrc/terrain $(INC_TILES) \
	    -c -o build/obj-wasm/FBTerrainLoader.o; \
	  emcc src/fdm/jsbsim_adapter.cpp -std=c++17 -O2 $(EMCC_WARN) -fexceptions \
	    -include src/fdm/em_compat.h -I$(JSBSIM)/src -Isrc/fdm -Isrc/core -c -o build/obj-wasm/jsbsim_adapter.o; \
	  emcc src/core/FBLog.cpp -std=c++17 -O2 $(EMCC_WARN) $(INC_MODULES) \
	    -c -o build/obj-wasm/FBLog.o; \
	  emcc src/core/FBTelemetry.cpp -std=c++17 -O2 $(EMCC_WARN) $(INC_MODULES) \
	    -c -o build/obj-wasm/FBTelemetry.o; \
	  emcc src/systems/FBAutopilot.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) -Isrc/terrain \
	    -c -o build/obj-wasm/FBAutopilot.o; \
	  emcc src/systems/FBFlightControl.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBFlightControl.o; \
	  emcc src/world/FBTerrainField.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) -Isrc/terrain \
	    -c -o build/obj-wasm/FBTerrainField.o; \
	  emcc src/modules/f16/FBF16Max7456.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16Max7456.o; \
	  emcc src/modules/f16/FBF16Module.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16Module.o; \
	  emcc src/systems/FBDisplaySystem.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBDisplaySystem.o; \
	  emcc src/systems/FBAirDataSystem.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBAirDataSystem.o; \
	  emcc src/systems/FBRadarAltimeter.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBRadarAltimeter.o; \
	  emcc src/systems/FBNavSystem.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBNavSystem.o; \
	  emcc src/modules/f16/FBF16FireControl.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16FireControl.o; \
	  emcc src/modules/f16/FBF16Ufc.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16Ufc.o; \
	  emcc src/modules/f16/FBF16Sms.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16Sms.o; \
	  emcc src/modules/f16/displays/FBF16Hud.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16Hud.o; \
	  emcc src/units/FBOwnshipUnit.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBOwnshipUnit.o; \
	  emcc src/systems/FBAirframeControls.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBAirframeControls.o; \
	  emcc src/systems/FBPilot.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBPilot.o; \
	  emcc src/modules/f16/FBF16Pilot.cpp -std=c++17 -O2 $(EMCC_WARN) -Isrc/fdm $(INC_MODULES) \
	    -c -o build/obj-wasm/FBF16Pilot.o; \
	  emcc src/core/FBMissionFile.cpp -std=c++17 -O2 $(EMCC_WARN) $(INC_MODULES) \
	    -c -o build/obj-wasm/FBMissionFile.o; \
	  emcc src/core/FBFlightMonitor.cpp -std=c++17 -O2 $(EMCC_WARN) $(INC_MODULES) \
	    -c -o build/obj-wasm/FBFlightMonitor.o; \
	  emcc build/obj-wasm/*.o $(JSB_WASM)/src/libJSBSim.a \
	    src/app/FBAppWasm.cpp src/app/FBLogSinks.cpp src/render/FBRenderer.cpp src/render/FBHudGeometry.cpp src/world/FBWorld.cpp \
	    src/render/stages/FBStarsStage.cpp src/render/stages/FBTileLightsStage.cpp \
	    src/render/stages/FBHudStage.cpp src/render/stages/FBUpscaleStage.cpp \
	    src/render/stages/FBTransmittanceStage.cpp src/render/stages/FBSkyViewStage.cpp \
	    src/render/stages/FBSkyStage.cpp src/render/stages/FBSunStage.cpp src/render/stages/FBMoonStage.cpp \
	    src/render/stages/FBTilesStage.cpp \
	    src/render/stages/FBCloudMipDownStage.cpp src/render/stages/FBCloudBaseBakeStage.cpp \
	    src/render/stages/FBCloudDetailBakeStage.cpp src/render/stages/FBCloudCellBakeStage.cpp \
	    src/render/stages/FBCloudMarchStage.cpp src/render/stages/FBCloudResolveStage.cpp \
	    src/render/stages/FBTonemapStage.cpp \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O1 $(EMCC_WARN) -fexceptions \
	    $(INC_WASM) \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_fb_toggle_ground,_fb_set_ground \
	    --embed-file $(JSBSIM)/aircraft/f16@/jsbsim/aircraft/f16 \
	    --embed-file $(JSBSIM)/engine/F100-PW-229.xml@/jsbsim/engine/F100-PW-229.xml \
	    --embed-file $(JSBSIM)/engine/direct.xml@/jsbsim/engine/direct.xml \
	    --embed-file web/moon.jpg@/moon.jpg \
	    -o web/gpu.js; \
	  echo "WASM -> web/gpu.js + web/fbtileworker.js (served at /)"

worker:          ## build the off-thread tile worker (fetch/decode/mesh/mips) -> web/fbtileworker.js
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  mkdir -p build/obj-worker; \
	  for f in src/terrain/*.cpp; do o=build/obj-worker/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) -Isrc/terrain -Isrc/core -Ivendor -c -o "$$o"; done; \
	  emcc src/core/FBLog.cpp $(CXXSTD) -O2 $(EMCC_WARN) -Isrc/core -c -o build/obj-worker/FBLog.o; \
	  emcc build/obj-worker/*.o src/app/FBTileWorkerMain.cpp src/app/FBLogSinks.cpp $(CXXSTD) -O2 $(EMCC_WARN) \
	    -Isrc/render -Isrc/terrain -Isrc/core $(INC_TILES) \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sEXIT_RUNTIME=0 -sFETCH \
	    -sEXPORTED_FUNCTIONS=_malloc,_free,_fbtw_open,_fbtw_build,_fbtw_verts,_fbtw_nverts,_fbtw_err,_fbtw_origin,_fbtw_mips,_fbtw_mipbytes,_fbtw_ts,_fbtw_release \
	    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,HEAPF32,HEAPF64 \
	    -o web/fbtileworker.js; \
	  echo "WASM worker -> web/fbtileworker.js"

core-lib:        ## build the FlightBox core library (sim+missions+FDM, no render/world) -> build/libfbcore.a
	@cd $(SELF_DIR); set -e; \
	  test -f $(JSB_HOST)/lib/libJSBSim.a || bash vendor/build_jsbsim_host.sh; \
	  mkdir -p build/obj-core; \
	  o=build/obj-core/jsbsim_adapter.o; f=src/fdm/jsbsim_adapter.cpp; \
	  [ "$$o" -nt "$$f" ] || c++ "$$f" -std=c++17 -O2 $(CXX_WARN) \
	    -Isrc/fdm -Isrc/core -isystem $(JSB_HOST)/include/JSBSim -c -o "$$o"; \
	  for f in $(filter-out src/fdm/jsbsim_adapter.cpp,$(CORE_LIB_SRCS)); do \
	    o=build/obj-core/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || c++ "$$f" -std=c++17 -O2 $(CXX_WARN) -Isrc/fdm $(INC_NATIVE) -c -o "$$o"; \
	  done; \
	  ar rcs build/libfbcore.a build/obj-core/*.o; \
	  echo "core-lib -> build/libfbcore.a ($$(ls build/obj-core/*.o | wc -l | tr -d ' ') objects)"

native: core-lib ## build the native headless WebGPU oracle (Dawn + real fb-tiles terrain + in-process F-16 for --fly) -> build/gpu_native
	@cd $(SELF_DIR); set -e; \
	  PLATFORM_LIBS=""; \
	  if [ "$$(uname -s)" = "Darwin" ]; then \
	    PLATFORM_LIBS="-framework Cocoa -framework IOKit -framework Foundation -framework IOSurface -framework QuartzCore -framework Metal"; \
	  fi; \
	  test -f $(DAWN_LIBDIR)/libwebgpu_dawn.a || bash vendor/build_dawn_native.sh; \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  mkdir -p build/obj-native; \
	  for f in src/terrain/*.cpp; do o=build/obj-native/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) -Isrc/terrain -Isrc/core -isystem vendor -c -o "$$o"; done; \
	  c++ src/world/FBTerrainLoader.cpp $(CXXSTD) -O2 $(CXX_WARN) $(INC_NATIVE) $(INC_TILES) \
	    -isystem $(CURL_COMPAT) -c -o build/obj-native/FBTerrainLoader.o; \
	  c++ src/world/FBTerrainField.cpp -std=c++17 -O2 $(CXX_WARN) -Isrc/fdm $(INC_NATIVE) \
	    -c -o build/obj-native/FBTerrainField.o; \
	  c++ src/app/FBAppNative.cpp src/app/FBMissionRunner.cpp src/app/FBLogSinks.cpp src/app/FBTelemetrySinks.cpp \
	    src/render/FBRenderer.cpp src/world/FBWorld.cpp \
	    src/render/stages/FBStarsStage.cpp src/render/stages/FBTileLightsStage.cpp \
	    src/render/stages/FBHudStage.cpp src/render/stages/FBUpscaleStage.cpp \
	    src/render/stages/FBTransmittanceStage.cpp src/render/stages/FBSkyViewStage.cpp \
	    src/render/stages/FBSkyStage.cpp src/render/stages/FBSunStage.cpp src/render/stages/FBMoonStage.cpp \
	    src/render/stages/FBTilesStage.cpp \
	    src/render/stages/FBCloudMipDownStage.cpp src/render/stages/FBCloudBaseBakeStage.cpp \
	    src/render/stages/FBCloudDetailBakeStage.cpp src/render/stages/FBCloudCellBakeStage.cpp \
	    src/render/stages/FBCloudMarchStage.cpp src/render/stages/FBCloudResolveStage.cpp \
	    src/render/stages/FBTonemapStage.cpp \
	    build/obj-native/*.o build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++20 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor \
	    -isystem $(CURL_COMPAT) -isystem $(JSB_HOST)/include/JSBSim \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm $$PLATFORM_LIBS \
	    -o build/gpu_native; \
	  echo "native -> build/gpu_native"

gym: core-lib    ## build the headless mission client (Mission in, Telemetrie raus; NO GPU/render link) -> build/fb-gym
	@cd $(SELF_DIR); set -e; \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  mkdir -p build/obj-gym; \
	  for f in src/terrain/*.cpp; do o=build/obj-gym/$$(basename $$f .cpp).o; \
	    [ "$$o" -nt "$$f" ] || c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) -Isrc/terrain -Isrc/core -isystem vendor -c -o "$$o"; done; \
	  c++ src/world/FBTerrainLoader.cpp $(CXXSTD) -O2 $(CXX_WARN) $(INC_NATIVE) $(INC_TILES) \
	    -isystem $(CURL_COMPAT) -c -o build/obj-gym/FBTerrainLoader.o; \
	  c++ src/app/FBAppGym.cpp src/app/FBMissionRunner.cpp src/app/FBLogSinks.cpp src/app/FBTelemetrySinks.cpp \
	    build/obj-gym/*.o build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++17 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) -isystem $(CURL_COMPAT) -isystem $(JSB_HOST)/include/JSBSim \
	    -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm \
	    -o build/fb-gym; \
	  echo "gym -> build/fb-gym"

test-monitor: core-lib   ## build FBFlightMonitor's negative-proof harnesses -> build/fb-test-hard-landing, build/fb-test-loc-departure
	@cd $(SELF_DIR); set -e; \
	  c++ src/app/FBTestHardLanding.cpp src/app/FBLogSinks.cpp \
	    build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++17 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) -isystem $(JSB_HOST)/include/JSBSim \
	    -lpthread -ldl -lm \
	    -o build/fb-test-hard-landing; \
	  c++ src/app/FBTestLocDeparture.cpp src/app/FBLogSinks.cpp \
	    build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++17 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) -isystem $(JSB_HOST)/include/JSBSim \
	    -lpthread -ldl -lm \
	    -o build/fb-test-loc-departure; \
	  echo "test-monitor -> build/fb-test-hard-landing build/fb-test-loc-departure"

image:           ## build the deployable fb-sim web container (context = sim/, trimmed by .containerignore)
	cd $(SELF_DIR) && podman build -f Dockerfile -t fb-sim .

up:              ## run the web host container (fb-sim, :8080; sim runs IN the browser)
	@cd $(SELF_DIR) && bash up.sh

down:            ## stop the web host
	-podman rm -f fb-sim 2>/dev/null

restart: down up ## stop and start the web host

clean:           ## remove build artifacts (WASM + worker + core-lib + native + gym)
	cd $(SELF_DIR) && rm -rf build/obj-wasm build/obj-worker build/obj-core build/obj-native build/obj-gym \
	  build/libfbcore.a build/gpu_native build/fb-gym \
	  web/gpu.js web/gpu.wasm web/fbtileworker.js web/fbtileworker.wasm
