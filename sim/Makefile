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
#   make test-monitor  build FBFlightMonitors negative-proof harnesses (hard landing, LOC departure,
#                 numerical divergence) -> build/fb-test-{hard-landing,loc-departure,nan-divergence}
#   make test-fdm  build the two-instance FDM coexistence proof   -> build/fb-test-two-fdm
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

# Incremental compiles WITH header dependencies. The object-building recipes below are shell loops, not
# make pattern rules, so make's own prerequisite graph never sees a header: a plain `[ obj -nt src ]`
# guard silently keeps objects built against an OLD header (change FBFdm.h/FBState.h and the next
# `make gym` links stale code — and every number measured from that binary is a phantom). DEPFLAGS makes
# each compile write its full prerequisite list to <obj>.d; fb_uptodate re-reads it and rebuilds unless
# the object is newer than its source AND every header it actually included. ($$(...) command
# substitution, not backticks: inside backticks the shell eats one level of backslash even in single
# quotes, which mangles the sed script that strips the .d file's line continuations.)
DEPFLAGS := -MMD -MP
FB_UPTODATE := fb_uptodate() { o="$$1"; f="$$2"; d="$${o%.o}.d"; if [ ! -f "$$o" ] || [ ! -f "$$d" ] || [ "$$f" -nt "$$o" ]; then return 1; fi; for p in $$(sed -e 's/^[^:]*://' -e 's/\\//g' "$$d"); do if [ ! -e "$$p" ] || [ "$$p" -nt "$$o" ]; then return 1; fi; done; return 0; };

DAWN_OUT     := vendor/dawn/out
DAWN_LIBDIR  := $(DAWN_OUT)/src/dawn/native
CURL_COMPAT  := vendor/.compat-headers
JSBSIM       := vendor/jsbsim
JSB_WASM     := build/jsbsim-wasm
JSB_HOST     := build/jsbsim-host

# Shared include roots (flat -I per module dir; #include stays bare-filename). core = FBState/
# FBMode/telemetry/FBFlightPlan/FBRunway; math = value-math (FBMat4); render = FBRenderer + camera/mips/
# chunkmesh/ephemeris/the HUD's WebGPU-backend resources (FBHudGeometry, FBHudFont ROM); world = FBWorld/
# tile streaming; units = FBUnit/FBSimUnit (world-entity identity/pose + the whole flown unit); systems = the
# DEFAULT, airframe-agnostic guidance/FCS/displays/pilot/airframe-controls (FBAutopilot/
# FBFlightControl/FBDisplaySystem/FBPilot/FBAirframeControls) modules compose and may
# override; modules/f16 = the F-16's module (composes systems/ + its gain preset); fdm = the JSBSim
# adapter (namespace FlightBox, ONE-TU JSBSim seam, one FBFdm per airframe — see fdm/FBFdm.h); terrain = the lean
# terrain API (flat); vendor/jsbsim/src = the JSBSim headers.
INC_MODULES := -Isrc/core -Isrc/math -Isrc/render -Isrc/world -Isrc/units -Isrc/systems -Isrc/modules -Isrc/modules/f16 -Isrc/modules/f16/displays
INC_WASM   := $(INC_MODULES) -Isrc/fdm -Isrc/terrain -I$(JSBSIM)/src
INC_NATIVE := $(INC_MODULES) -Isrc/fdm -Isrc/terrain
INC_TILES := -I../tiles/src

# EVERY WebGPU render stage, one class per shader (CLAUDE.md render/stages). A wildcard, not a hand list:
# the wasm and native link lines used to carry two byte-identical copies of it, so adding a stage meant
# remembering both and a browser-only link error was the failure mode. Now a new file in the directory is
# a new stage in BOTH targets, by construction.
RENDER_STAGE_SRCS := $(wildcard src/render/stages/*.cpp)

# The FlightBox core library (Auftrag "Lib/Client-Split"): core/ + fdm/ + systems/ + modules/f16/ +
# units/ + libJSBSim, EVERYTHING a headless mission run needs and NOTHING render/world-streaming-
# specific (CLAUDE.md: "render/ und world/-Streaming gehoeren NICHT zur Core-Lib"). ONE exception:
# render/FBHudGeometry.cpp — physically under render/ but pure CPU vertex-list math (no WebGPU/Dawn
# include, verified), the one render/ file systems/FBDisplaySystem.cpp structurally depends on to build
# the HUD's vertex buffers; core-lib links it so fb-gym can, without pulling in any Dawn symbol (see the
# gym target's own nm check). This library is a NATIVE-TOOLCHAIN archive (build/libfbcore.a) shared by
# native + gym; the WASM client is a different toolchain entirely (emcc/wasm32) and necessarily
# RE-COMPILES this same list itself in the wasm target below — cross-compilation, not duplication.
# ONE LIST, LITERALLY: the wasm target loops over THIS variable. It used to keep a hand-written copy that
# had silently drifted by five translation units (FBModuleRegistry, FBF16ModuleRegistration,
# FBFdmTelemetrySource, FBRunwayPlateauElevation, FBBakedDemElevation were core-lib-only), i.e. the
# browser was built from a different simulator than fb-gym. Per-TU FLAGS may differ per toolchain (FBFdm
# needs -fexceptions + em_compat.h under emcc); the LIST cannot.
CORE_LIB_SRCS := \
  src/fdm/FBFdm.cpp src/fdm/FBFdmBoot.cpp src/fdm/FBFdmTelemetrySource.cpp \
  src/core/FBLog.cpp src/core/FBTelemetry.cpp src/core/FBMissionFile.cpp \
  src/core/FBRunwayPlateauElevation.cpp src/core/FBBakedDemElevation.cpp src/core/FBFlightMonitor.cpp \
  src/core/FBMissionMonitor.cpp \
  src/systems/FBAutopilot.cpp src/systems/FBFlightControl.cpp src/systems/FBDisplaySystem.cpp \
  src/systems/FBAirDataSystem.cpp src/systems/FBRadarAltimeter.cpp src/systems/FBNavSystem.cpp \
  src/systems/FBAirframeControls.cpp src/systems/FBPilot.cpp src/systems/FBDatalinkSystem.cpp \
  src/systems/FBRadarSystem.cpp \
  src/modules/FBModuleRegistry.cpp \
  src/modules/f16/FBF16Fcr.cpp \
  src/modules/f16/FBF16Max7456.cpp src/modules/f16/FBF16Module.cpp src/modules/f16/FBF16FireControl.cpp \
  src/modules/f16/FBF16Ufc.cpp src/modules/f16/FBF16Sms.cpp src/modules/f16/FBF16Pilot.cpp \
  src/modules/f16/FBF16ModuleRegistration.cpp \
  src/modules/f16/displays/FBF16Hud.cpp \
  src/units/FBSimUnit.cpp \
  src/render/FBHudGeometry.cpp

.PHONY: help wasm worker core-lib native gym test-monitor test-fdm image up down restart clean

help:            ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

wasm: worker     ## build the WASM Command Center + tile worker -> web/gpu.{js,wasm} + web/fbtileworker.{js,wasm}
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  $(FB_UPTODATE) \
	  	  test -f $(JSB_WASM)/src/libJSBSim.a || bash vendor/build_jsbsim_wasm.sh; \
	  mkdir -p build/obj-wasm; \
	  for f in src/terrain/*.cpp; do o=build/obj-wasm/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) -Isrc/terrain -Isrc/core -Ivendor -c -o "$$o"; done; \
	  emcc src/world/FBTerrainLoader.cpp $(CXXSTD) -O1 $(EMCC_WARN) $(INC_MODULES) -Isrc/terrain $(INC_TILES) \
	    -c -o build/obj-wasm/FBTerrainLoader.o; \
	  o=build/obj-wasm/FBFdm.o; f=src/fdm/FBFdm.cpp; \
	  fb_uptodate "$$o" "$$f" || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) -fexceptions \
	    -include src/fdm/em_compat.h -I$(JSBSIM)/src -Isrc/fdm -Isrc/core -c -o "$$o"; \
	  for f in $(filter-out src/fdm/FBFdm.cpp,$(CORE_LIB_SRCS)); do \
	    o=build/obj-wasm/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) -Isrc/fdm $(INC_MODULES) -Isrc/terrain -c -o "$$o"; \
	  done; \
	  emcc build/obj-wasm/*.o $(JSB_WASM)/src/libJSBSim.a \
	    src/app/FBAppWasm.cpp src/app/FBLogSinks.cpp src/render/FBRenderer.cpp src/world/FBWorld.cpp \
	    $(RENDER_STAGE_SRCS) \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O1 $(EMCC_WARN) -fexceptions \
	    $(INC_WASM) \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_fb_toggle_ground,_fb_set_ground \
	    --embed-file $(JSBSIM)/aircraft/f16@/jsbsim/aircraft/f16 \
	    --embed-file $(JSBSIM)/engine/F100-PW-229.xml@/jsbsim/engine/F100-PW-229.xml \
	    --embed-file $(JSBSIM)/engine/direct.xml@/jsbsim/engine/direct.xml \
	    --embed-file web/moon.jpg@/moon.jpg \
	    -o web/gpu.js; \
	  mkdir -p web/missions; cp missions/*.fbm web/missions/; \
	  echo "WASM -> web/gpu.js + web/fbtileworker.js + web/missions/ (served at /)"

worker:          ## build the off-thread tile worker (fetch/decode/mesh/mips) -> web/fbtileworker.js
	@cd $(SELF_DIR); source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  $(FB_UPTODATE) \
	  	  mkdir -p build/obj-worker; \
	  for f in src/terrain/*.cpp; do o=build/obj-worker/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || emcc "$$f" $(CXXSTD) -O2 $(EMCC_WARN) $(DEPFLAGS) -Isrc/terrain -Isrc/core -Ivendor -c -o "$$o"; done; \
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
	  $(FB_UPTODATE) \
	  	  test -f $(JSB_HOST)/lib/libJSBSim.a || bash vendor/build_jsbsim_host.sh; \
	  mkdir -p build/obj-core; \
	  o=build/obj-core/FBFdm.o; f=src/fdm/FBFdm.cpp; \
	  fb_uptodate "$$o" "$$f" || c++ "$$f" -std=c++17 -O2 $(CXX_WARN) $(DEPFLAGS) \
	    -Isrc/fdm -Isrc/core -isystem $(JSB_HOST)/include/JSBSim -c -o "$$o"; \
	  for f in $(filter-out src/fdm/FBFdm.cpp,$(CORE_LIB_SRCS)); do \
	    o=build/obj-core/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || c++ "$$f" -std=c++17 -O2 $(CXX_WARN) $(DEPFLAGS) -Isrc/fdm $(INC_NATIVE) -c -o "$$o"; \
	  done; \
	  ar rcs build/libfbcore.a build/obj-core/*.o; \
	  echo "core-lib -> build/libfbcore.a ($$(ls build/obj-core/*.o | wc -l | tr -d ' ') objects)"

native: core-lib ## build the native headless WebGPU oracle (Dawn + real fb-tiles terrain + in-process F-16 for --fly) -> build/gpu_native
	@cd $(SELF_DIR); set -e; \
	  $(FB_UPTODATE) \
	  	  PLATFORM_LIBS=""; \
	  if [ "$$(uname -s)" = "Darwin" ]; then \
	    PLATFORM_LIBS="-framework Cocoa -framework IOKit -framework Foundation -framework IOSurface -framework QuartzCore -framework Metal"; \
	  fi; \
	  test -f $(DAWN_LIBDIR)/libwebgpu_dawn.a || bash vendor/build_dawn_native.sh; \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  mkdir -p build/obj-native; \
	  for f in src/terrain/*.cpp; do o=build/obj-native/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS) -Isrc/terrain -Isrc/core -isystem vendor -c -o "$$o"; done; \
	  c++ src/world/FBTerrainLoader.cpp $(CXXSTD) -O2 $(CXX_WARN) $(INC_NATIVE) $(INC_TILES) \
	    -isystem $(CURL_COMPAT) -c -o build/obj-native/FBTerrainLoader.o; \
	  c++ src/app/FBAppNative.cpp src/app/FBMissionRunner.cpp src/app/FBTickPool.cpp src/app/FBLogSinks.cpp src/app/FBTelemetrySinks.cpp \
	    src/render/FBRenderer.cpp src/world/FBWorld.cpp \
	    $(RENDER_STAGE_SRCS) \
	    build/obj-native/*.o build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++20 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem vendor \
	    -isystem $(CURL_COMPAT) -isystem $(JSB_HOST)/include/JSBSim \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm $$PLATFORM_LIBS \
	    -o build/gpu_native; \
	  echo "native -> build/gpu_native"

# The GPU-freeness claim in CLAUDE.md and in FBAppGym.cpp's banner is ENFORCED here, not asserted: the
# link is followed by an nm scan of the defined external symbols, and any wgpu*/dawn*/webgpu* match
# deletes the binary and fails the build. Without it, "verifiziert per nm" was a comment.
gym: core-lib    ## build the headless mission client (Mission in, Telemetrie raus; NO GPU/render link) -> build/fb-gym
	@cd $(SELF_DIR); set -e; \
	  $(FB_UPTODATE) \
	  	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  mkdir -p build/obj-gym; \
	  for f in src/terrain/*.cpp; do o=build/obj-gym/$$(basename $$f .cpp).o; \
	    fb_uptodate "$$o" "$$f" || c++ "$$f" $(CXXSTD) -O2 $(CXX_WARN) $(DEPFLAGS) -Isrc/terrain -Isrc/core -isystem vendor -c -o "$$o"; done; \
	  c++ src/world/FBTerrainLoader.cpp $(CXXSTD) -O2 $(CXX_WARN) $(INC_NATIVE) $(INC_TILES) \
	    -isystem $(CURL_COMPAT) -c -o build/obj-gym/FBTerrainLoader.o; \
	  c++ src/app/FBAppGym.cpp src/app/FBMissionRunner.cpp src/app/FBTickPool.cpp src/app/FBLogSinks.cpp src/app/FBTelemetrySinks.cpp \
	    build/obj-gym/*.o build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++17 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) -isystem $(CURL_COMPAT) -isystem $(JSB_HOST)/include/JSBSim \
	    -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm \
	    -o build/fb-gym; \
	  gpusyms=$$(nm -gU build/fb-gym 2>/dev/null | grep -Eic '(^|[^a-z])(wgpu|dawn)[A-Za-z_]|webgpu' || true); \
	  if [ "$$gpusyms" != "0" ]; then \
	    nm -gU build/fb-gym | grep -Ei '(^|[^a-z])(wgpu|dawn)[A-Za-z_]|webgpu' | head -20; \
	    rm -f build/fb-gym; \
	    echo "gym: FAILED — $$gpusyms Dawn/WebGPU symbol(s) leaked into the headless client" >&2; exit 1; \
	  fi; \
	  echo "gym -> build/fb-gym (GPU-free: 0 Dawn/WebGPU symbols)"

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
	  c++ src/app/FBTestNanDivergence.cpp src/app/FBLogSinks.cpp \
	    build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++17 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) -isystem $(JSB_HOST)/include/JSBSim \
	    -lpthread -ldl -lm \
	    -o build/fb-test-nan-divergence; \
	  echo "test-monitor -> build/fb-test-hard-landing build/fb-test-loc-departure build/fb-test-nan-divergence"

test-fdm: core-lib       ## build the two-instance FDM coexistence proof (multi-unit stage 1) -> build/fb-test-two-fdm
	@cd $(SELF_DIR); set -e; \
	  c++ src/app/FBTestTwoFdm.cpp src/app/FBLogSinks.cpp \
	    build/libfbcore.a $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++17 -O2 $(CXX_WARN) \
	    $(INC_NATIVE) -isystem $(JSB_HOST)/include/JSBSim \
	    -lpthread -ldl -lm \
	    -o build/fb-test-two-fdm; \
	  echo "test-fdm -> build/fb-test-two-fdm"

image:           ## build the deployable fb-sim web container (context = sim/, trimmed by .containerignore)
	cd $(SELF_DIR) && podman build -f Dockerfile -t fb-sim .

up:              ## run the web host container (fb-sim, :8080; sim runs IN the browser)
	@cd $(SELF_DIR) && bash up.sh

down:            ## stop the web host
	-podman rm -f fb-sim 2>/dev/null

restart: down up ## stop and start the web host

clean:           ## remove build artifacts (WASM + worker + core-lib + native + gym)
	cd $(SELF_DIR) && rm -rf build/obj-wasm build/obj-worker build/obj-core build/obj-native build/obj-gym \
	  build/libfbcore.a build/gpu_native build/fb-gym build/fb-test-hard-landing \
	  build/fb-test-loc-departure build/fb-test-two-fdm \
	  web/gpu.js web/gpu.wasm web/fbtileworker.js web/fbtileworker.wasm
