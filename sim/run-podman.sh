#!/usr/bin/env bash
# Build + run the two-container simulation on a podman network.
# The "radio" is UDP between the containers; browser talks to the flightbox.
set -euo pipefail
cd "$(dirname "$0")"

NET=flightboxnet
IMG_A=fb-aircraft
IMG_F=fb-flightbox

echo ">> Ensure the WASM is built (flightbox/web/cc.wasm)"
if [ ! -f flightbox/web/cc.wasm ]; then ./build-wasm.sh; fi

echo ">> network"
podman network exists "$NET" || podman network create "$NET"

echo ">> build images"
podman build -f aircraft/Containerfile  -t "$IMG_A" .
podman build -f flightbox/Containerfile -t "$IMG_F" .

echo ">> (re)start containers"
# Shared ENU origin = home (default Hameln). Set ORIGIN_LAT/ORIGIN_LON to fly anywhere;
# it drives both the aircraft home and the command-center osmmesh streaming.
OLAT="${ORIGIN_LAT:-52.045}"; OLON="${ORIGIN_LON:-9.385}"
podman rm -f "$IMG_A" "$IMG_F" >/dev/null 2>&1 || true
podman run -d --name "$IMG_F" --network "$NET" -e AIRCRAFT_ADDR="$IMG_A" \
    -e ORIGIN_LAT="$OLAT" -e ORIGIN_LON="$OLON" -p 8080:8080 "$IMG_F"
podman run -d --name "$IMG_A" --network "$NET" -e FLIGHTBOX_ADDR="$IMG_F" \
    -e ORIGIN_LAT="$OLAT" -e ORIGIN_LON="$OLON" -e FDM_MODEL="${FDM_MODEL:-1}" "$IMG_A"

echo
echo "== running =="
podman ps --filter name=fb- --format '  {{.Names}}  {{.Status}}'
echo
echo "Open  http://localhost:8080  in a browser."
echo "Stop: podman rm -f $IMG_A $IMG_F"
