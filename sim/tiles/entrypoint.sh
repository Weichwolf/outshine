#!/bin/bash
# Starts fb-tiles (origin, loopback:8082) + nginx (proxy_cache front door, :8081). If either dies,
# the container exits -- no silent half-alive state; the restart policy handles bringing it back.
set -e
mkdir -p /var/cache/fbtiles/nginx

TILES_BIND=127.0.0.1 TILES_PORT=8082 /usr/local/bin/fb-tiles &
FB_PID=$!

nginx -g 'daemon off;' &
NGINX_PID=$!

wait -n "$FB_PID" "$NGINX_PID"
EXIT=$?
kill "$FB_PID" "$NGINX_PID" 2>/dev/null || true
exit "$EXIT"
