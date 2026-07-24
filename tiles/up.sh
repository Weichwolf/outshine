#!/bin/sh
# Host launcher: (re)start the fb-tiles container on the persistent cache volume.
set -e
cd "$(dirname "$0")/.."
podman build -f tiles/Dockerfile -t fb-tiles .
podman rm -f fb-tiles 2>/dev/null || true
podman run -d --name fb-tiles -p 8081:8081 -v fbtiles-cache:/var/cache/fbtiles fb-tiles
echo "fb-tiles -> http://localhost:8081/health"
