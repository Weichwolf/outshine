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

.PHONY: help containers controlcenter client up down restart watch tests eeprom clean

help:                ## list targets
	@grep -hE '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | sed -E 's/:[^#]*##/\t/' | sort | column -t -s $$'\t'

containers: controlcenter  ## build the images: the sim (JSBSim+iNav+hub) + tiles (needs the WASM CC first)
	podman build -f flightbox/Containerfile -t fb-flightbox .
	podman build -f tiles/Containerfile     -t fb-tiles     .

controlcenter:       ## build the WASM browser Command Center (cc.wasm + tile worker) -> flightbox/web/
	@source "$(EMSDK_DIR)/emsdk_env.sh" >/dev/null 2>&1; set -e; \
	  emcc command_center/cc.c geo/osmmesh/src/*.c \
	    -O2 $(EMCC_WARN) -Icommon -Igeo/osmmesh/include \
	    -sUSE_SDL=2 -lwebsocket.js -sFULL_ES2 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 \
	    -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=128MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=0 \
	    -o flightbox/web/cc.js; \
	  echo "WASM -> flightbox/web/cc.js (+ cc.wasm)"; \
	  emcc command_center/tileworker.c geo/osmmesh/src/*.c \
	    -O2 $(EMCC_WARN) -Icommon -Igeo/osmmesh/include -Icommand_center \
	    -sFETCH -sASYNCIFY -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB -sEXIT_RUNTIME=0 \
	    -sMODULARIZE=1 -sEXPORT_NAME=TileWorker -sENVIRONMENT=worker \
	    -sEXPORTED_RUNTIME_METHODS=ccall,HEAPF32,HEAPF64 \
	    -o flightbox/web/tw.js; \
	  echo "WASM -> flightbox/web/tw.js (+ tw.wasm)"

client:              ## build the headless TypeScript Command Center (cc/dist)
	cd cc && tsc -p tsconfig.json && echo "TS CC -> cc/dist/"

eeprom:              ## (re)generate the iNav eeprom for AC (c172|sgs233|f16)
	bash aircraft/make-eeprom.sh $(AC)

up:                  ## run the ONE persistent sim container (AC=c172|sgs233|f16)
	FB_TIME_SCALE=$(FB_TIME_SCALE) bash flightbox-up.sh $(AC)

down:                ## stop the sim container (tiles keeps running)
	-podman rm -f fb-flightbox 2>/dev/null

restart: down up     ## stop and start the sim container

watch: client        ## stream telemetry from the running hub (a read-only command center)
	node cc/dist/bin/cc.js watch

tests: client        ## run the declarative mission tests against the running stack
	node cc/dist/bin/harness.js $(SPEC)

clean:               ## remove build artifacts (WASM + TS dist)
	rm -f flightbox/web/cc.js flightbox/web/cc.wasm flightbox/web/tw.js flightbox/web/tw.wasm
	rm -rf cc/dist
