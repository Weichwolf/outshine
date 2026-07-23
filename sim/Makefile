# FlightBox — one entry point for building and running the simulator.
#
# Everything runs in CONTAINERS; only one flightbox + one aircraft instance runs at a time, and any
# number of command centers (the WASM browser CC, the headless TS CC, tests) connect to the hub in
# parallel. Host tools: podman, emscripten (WASM CC), tsc (TS CC), python3 (mission resolve).
#
#   make containers      build the container images (aircraft, flightbox hub, tiles)
#   make controlcenter   build the WASM browser Command Center  (-> flightbox/web/)
#   make client          build the headless TypeScript CC        (-> cc/dist/)
#   make up AC=c172      run the ONE persistent flightbox + aircraft (c172|sgs233|f16)
#   make watch           stream telemetry from the running hub (a read-only CC)
#   make tests           run the declarative mission tests against the running stack
#   make down            stop the stack
SHELL := /bin/bash
.DEFAULT_GOAL := help
EMSDK_DIR ?= $(HOME)/Git/emsdk
AC ?= c172
FB_TIME_SCALE ?= 1
EMCC_WARN := -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter

.PHONY: help containers controlcenter webgpu webgpu-worker webgpu-native client up down restart watch tests eeprom clean

help:                ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

containers: controlcenter  ## build the images: the sim (JSBSim+iNav+hub) + tiles (needs the WASM CC first)
	podman build -f flightbox/Containerfile -t fb-flightbox .
	podman build -f tiles/Containerfile     -t fb-tiles     .

# _free MUST stay in EXPORTED_FUNCTIONS: the tilesrc_js.h worker glue calls _malloc/_free as JS globals;
# since the libJSBSim link emcc no longer exports _free by default -> ReferenceError killed the tile pump.
controlcenter:       ## build the WASM browser Command Center (cc.wasm + tile worker) -> flightbox/web/
	@source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  test -f validator/build/jsbsim-wasm/src/libJSBSim.a || { echo "libJSBSim.a (WASM) missing — run: bash validator/build_jsbsim_wasm.sh"; exit 1; }; \
	  mkdir -p build/osmmesh-wasm; \
	  for f in geo/osmmesh/src/*.c; do o=build/osmmesh-wasm/$$(basename $$f .c).o; \
	    [ "$$o" -nt "$$f" ] || emcc "$$f" -O2 $(EMCC_WARN) -Igeo/osmmesh/include -c -o "$$o"; done; \
	  emcc command_center/cc.cpp aircraft/fdm/jsbsim_adapter.cpp \
	    command_center/fb/FBAutopilot.cpp command_center/fb/FBFlightControl.cpp \
	    build/osmmesh-wasm/*.o \
	    validator/build/jsbsim-wasm/src/libJSBSim.a \
	    -std=c++17 -O2 $(EMCC_WARN) -Icommon -Igeo/osmmesh/include -Ijsbsim/src -Iaircraft/fdm -Icommand_center \
	    -fexceptions -include aircraft/fdm/em_compat.h \
	    -sUSE_SDL=2 -lwebsocket.js -sFULL_ES2 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 \
	    -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free \
	    --embed-file jsbsim/aircraft/f16@/jsbsim/aircraft/f16 \
	    --embed-file jsbsim/engine/F100-PW-229.xml@/jsbsim/engine/F100-PW-229.xml \
	    --embed-file jsbsim/engine/direct.xml@/jsbsim/engine/direct.xml \
	    -o flightbox/web/cc.js; \
	  echo "WASM -> flightbox/web/cc.js (+ cc.wasm)"; \
	  emcc command_center/tileworker.c geo/osmmesh/src/*.c \
	    -O2 $(EMCC_WARN) -Icommon -Igeo/osmmesh/include -Icommand_center \
	    -sFETCH -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sEXIT_RUNTIME=0 \
	    -sMODULARIZE=1 -sEXPORT_NAME=TileWorker -sENVIRONMENT=worker \
	    -sEXPORTED_RUNTIME_METHODS=ccall,HEAPF32,HEAPF64 \
	    -o flightbox/web/tw.js; \
	  echo "WASM -> flightbox/web/tw.js (+ tw.wasm)"

webgpu:              ## build the WebGPU demo page (real F-16 sim + streamed terrain) -> flightbox/web/gpu.js
	@source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  test -f validator/build/jsbsim-wasm/src/libJSBSim.a || { echo "libJSBSim.a (WASM) missing — run: bash validator/build_jsbsim_wasm.sh"; exit 1; }; \
	  mkdir -p build/osmmesh-wasm; \
	  for f in geo/osmmesh/src/*.c; do o=build/osmmesh-wasm/$$(basename $$f .c).o; \
	    [ "$$o" -nt "$$f" ] || emcc "$$f" -O2 $(EMCC_WARN) -Igeo/osmmesh/include -c -o "$$o"; done; \
	  emcc command_center/fb/fb_terrain.c -O1 $(EMCC_WARN) \
	    -Icommand_center/fb -Icommand_center -Igeo/osmmesh/include -c -o build/osmmesh-wasm/fb_terrain.o; \
	  emcc aircraft/fdm/jsbsim_adapter.cpp -std=c++17 -O2 $(EMCC_WARN) -fexceptions \
	    -include aircraft/fdm/em_compat.h -Icommon -Ijsbsim/src -Iaircraft/fdm -c -o build/osmmesh-wasm/jsbsim_adapter.o; \
	  emcc command_center/fb/FBAutopilot.cpp -std=c++17 -O2 -Wall -Werror -Iaircraft/fdm -Icommand_center/fb \
	    -c -o build/osmmesh-wasm/FBAutopilot.o; \
	  emcc command_center/fb/FBFlightControl.cpp -std=c++17 -O2 -Wall -Werror -Iaircraft/fdm -Icommand_center/fb \
	    -c -o build/osmmesh-wasm/FBFlightControl.o; \
	  emcc build/osmmesh-wasm/*.o validator/build/jsbsim-wasm/src/libJSBSim.a \
	    command_center/fb/gpu_main.cpp command_center/fb/FBRenderer.cpp command_center/fb/FBWorld.cpp \
	    --use-port=vendor/emdawnwebgpu_pkg/emdawnwebgpu.port.py -std=c++20 -O1 -Wall -Werror -fexceptions \
	    -Icommand_center/fb -Icommand_center -Igeo/osmmesh/include -Ijsbsim/src -Iaircraft/fdm \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=0 \
	    -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_fb_toggle_ground,_fb_set_ground \
	    --embed-file jsbsim/aircraft/f16@/jsbsim/aircraft/f16 \
	    --embed-file jsbsim/engine/F100-PW-229.xml@/jsbsim/engine/F100-PW-229.xml \
	    --embed-file jsbsim/engine/direct.xml@/jsbsim/engine/direct.xml \
	    --embed-file flightbox/web/moon.jpg@/moon.jpg \
	    -o flightbox/web/gpu.js; \
	  echo "WASM -> flightbox/web/gpu.js (open /gpu.html)"

webgpu-worker:       ## build the off-thread tile worker (fetch/decode/mesh/mips) -> flightbox/web/fbtileworker.js
	@source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  mkdir -p build/osmmesh-worker; \
	  for f in geo/osmmesh/src/*.c; do o=build/osmmesh-worker/$$(basename $$f .c).o; \
	    [ "$$o" -nt "$$f" ] || emcc "$$f" -O2 $(EMCC_WARN) -Igeo/osmmesh/include -c -o "$$o"; done; \
	  emcc build/osmmesh-worker/*.o command_center/fb/fbtileworker.c -O2 $(EMCC_WARN) \
	    -Icommand_center/fb -Icommand_center -Igeo/osmmesh/include \
	    -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sEXIT_RUNTIME=0 -sFETCH \
	    -sEXPORTED_FUNCTIONS=_malloc,_free,_fbtw_open,_fbtw_build,_fbtw_verts,_fbtw_nverts,_fbtw_err,_fbtw_origin,_fbtw_mips,_fbtw_mipbytes,_fbtw_ts,_fbtw_release \
	    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPU8,HEAPF32,HEAPF64 \
	    -o flightbox/web/fbtileworker.js; \
	  echo "WASM worker -> flightbox/web/fbtileworker.js"

# CURL_COMPAT: no libcurl-dev on this host (no curl.h, no libcurl.so symlink) and no sudo — same
# no-root apt-get-download+dpkg-deb trick as build_dawn_native.sh's X11 header note.
DAWN_OUT     := vendor/dawn/out
DAWN_LIBDIR  := $(DAWN_OUT)/src/dawn/native
CURL_COMPAT  := vendor/.compat-headers
JSB_HOST     := validator/build/jsbsim-host

webgpu-native:       ## build the native headless WebGPU CLI (Dawn + real fb-tiles terrain + in-process F-16 for --fly) -> build/gpu_native
	@test -f $(DAWN_LIBDIR)/libwebgpu_dawn.a || bash vendor/build_dawn_native.sh; \
	  test -f $(CURL_COMPAT)/curl/curl.h || bash vendor/fetch_curl_compat.sh; \
	  test -f $(JSB_HOST)/lib/libJSBSim.a || bash validator/build.sh; \
	  mkdir -p build/osmmesh-native; set -e; \
	  for f in geo/osmmesh/src/*.c; do o=build/osmmesh-native/$$(basename $$f .c).o; \
	    [ "$$o" -nt "$$f" ] || cc "$$f" -O2 -Wall -Igeo/osmmesh/include -c -o "$$o"; done; \
	  cc command_center/fb/fb_terrain.c -O2 -Wall -Wextra -Werror \
	    -Icommand_center/fb -Icommand_center -Igeo/osmmesh/include -isystem $(CURL_COMPAT) \
	    -c -o build/osmmesh-native/fb_terrain.o; \
	  c++ aircraft/fdm/jsbsim_adapter.cpp -std=c++17 -O2 -Wall \
	    -Iaircraft/fdm -isystem $(JSB_HOST)/include/JSBSim \
	    -c -o build/osmmesh-native/jsbsim_adapter.o; \
	  c++ command_center/fb/FBAutopilot.cpp -std=c++17 -O2 -Wall -Werror -Iaircraft/fdm -Icommand_center/fb \
	    -c -o build/osmmesh-native/FBAutopilot.o; \
	  c++ command_center/fb/FBFlightControl.cpp -std=c++17 -O2 -Wall -Werror -Iaircraft/fdm -Icommand_center/fb \
	    -c -o build/osmmesh-native/FBFlightControl.o; \
	  c++ command_center/fb/gpu_native.cpp command_center/fb/FBRenderer.cpp command_center/fb/FBWorld.cpp \
	    build/osmmesh-native/*.o $(JSB_HOST)/lib/libJSBSim.a \
	    -std=c++20 -O2 -Wall -Wextra -Werror \
	    -Icommand_center/fb -Icommand_center -Igeo/osmmesh/include -Iaircraft/fdm \
	    -isystem $(DAWN_OUT)/gen/include -isystem vendor/dawn/include -isystem geo/osmmesh/src/3rdparty \
	    -isystem $(CURL_COMPAT) -isystem $(JSB_HOST)/include/JSBSim \
	    -L$(DAWN_LIBDIR) -lwebgpu_dawn -L$(CURL_COMPAT)/lib -lcurl -lpthread -ldl -lm \
	    -o build/gpu_native; \
	  echo "native -> build/gpu_native"

client:              ## build the headless TypeScript Command Center (cc/dist)
	cd cc && tsc -p tsconfig.json && echo "TS CC -> cc/dist/"

eeprom:              ## (re)generate the iNav eeprom for AC (c172|sgs233|f16)
	bash aircraft/make-eeprom.sh $(AC)

up:                  ## run the web host container (fb-web, :8080; sim runs IN the browser)
	bash web-up.sh

down:                ## stop the web host (fb-tiles keeps running)
	-podman rm -f fb-web 2>/dev/null

restart: down up     ## stop and start the sim container

watch: client        ## stream telemetry from the running hub (a read-only command center)
	node cc/dist/bin/cc.js watch

tests: client validator  ## fail-fast fly every declarative mission through the fast validator
	node cc/dist/bin/cc.js run-all $(SPEC)

validator:           ## build the host-native fast mission validator (libJSBSim + extracted iNav control)
	@bash validator/build.sh

clean:               ## remove build artifacts (WASM + TS dist)
	rm -f flightbox/web/cc.js flightbox/web/cc.wasm flightbox/web/tw.js flightbox/web/tw.wasm build/gpu_native
	rm -rf cc/dist
