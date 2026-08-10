# fb-tiles: the world-data service. Dynamically fetches and prepares real-world data
# (DEM now; vector/imagery next) and caches it, so neither the engine nor the renderer
# ships a preloaded region. tiles/ is self-contained (Makefile, Dockerfile, source, vendored
# osmmesh); build context = REPO ROOT (`podman build -f tiles/Dockerfile -t fb-tiles .`), so this
# COPYs tiles/ as one tree -- osmmesh lives nested inside it, not a sibling fetched separately.
#
# Two stages: compile fb-tiles in a throwaway build image (gcc + -dev headers), then copy just the
# binary into the final image, which adds nginx as a caching front door (tiles/nginx.conf) instead
# of the compiler toolchain -- smaller final image, and nginx's proxy_cache means the origin only
# ever sees a cold tile once no matter how many clients ask, wherever this is deployed centrally.
FROM docker.io/library/debian:trixie-slim AS build
# libcurl4-openssl-dev is the HEADER only: libcurl.so.4 was already here (curl pulls it in). The
# fetch path used to shell out to the curl BINARY once per tile -- fork, exec, TLS handshake, no
# connection reuse; measured 2.34x slower than one kept connection (see cache.c for the A/B and why
# the first version of it was worthless). So this is a link, not a new dependency: the library was
# always installed, we were just starting it as a process.
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY tiles/ tiles/
# osmmesh/ is flat (mvt.h etc. sit directly in it) and already trimmed to exactly what this server
# needs (see tiles/osmmesh/osmmesh.h) -- `-Itiles` resolves `<osmmesh/mvt.h>` as tiles/osmmesh/mvt.h.
RUN gcc -O2 -Wall -Wextra -Wpedantic -Werror -Itiles \
        tiles/src/main.c tiles/src/elev.c tiles/src/cache.c tiles/src/route.c tiles/src/tilesrc.c tiles/src/prefetch.c tiles/src/raster.c tiles/src/bake.c tiles/src/bakepool.c tiles/src/tilemap.c tiles/src/stars.c tiles/src/grib2.c tiles/src/wx.c tiles/src/peaks.c tiles/osmmesh/*.c \
        -o /usr/local/bin/fb-tiles -lm -lpthread -lcurl

FROM docker.io/library/debian:trixie-slim
# nginx-light: proxy_cache is core nginx (ngx_http_proxy_module + the cache machinery), not an
# extras-only module, so the light package is enough. libcurl4 here is the RUNTIME lib fb-tiles
# links against (no -dev headers needed in this stage, nothing gets compiled here). curl the BINARY
# stays for the same reason it always has: the only way to debug an upstream fetch from inside the
# container, and this image is where "why did that tile not arrive" gets asked.
RUN apt-get update && apt-get install -y --no-install-recommends nginx-light curl ca-certificates libcurl4 \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /var/cache/fbtiles
COPY --from=build /usr/local/bin/fb-tiles /usr/local/bin/fb-tiles
# Star catalogue: bounded 53 KB natural constant, baked by tiles/stars/build_stars.py and committed
# (deterministic, offline image build -- no python/network in the build). Served from /t/stars/z/0/0.
COPY tiles/stars/ /usr/local/share/fb-stars/
COPY tiles/nginx.conf /etc/nginx/nginx.conf
COPY tiles/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
EXPOSE 8081
ENTRYPOINT ["/entrypoint.sh"]
