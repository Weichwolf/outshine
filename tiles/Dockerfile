# fb-tiles: the world-data service. Dynamically fetches and prepares real-world data
# (DEM now; vector/imagery next) and caches it, so neither the engine nor the renderer
# ships a preloaded region. Build context = REPO ROOT
# (`podman build -f tiles/Dockerfile -t fb-tiles .`), because the decoders it links are the
# library's and live under src/.
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
RUN apt-get update && apt-get install -y --no-install-recommends g++ make libc6-dev \
        libcurl4-openssl-dev libsdl3-image-dev pkg-config \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
# THE DECLARED SOURCES AND NOTHING ELSE: `COPY tiles/` also carries a host-built tiles/fb-tiles --
# gitignored, so invisible -- and make then finds the target current and installs it unbuilt.
# The library comes along because there is one decoder, not two: this server reads the same MVT
# reader the client does (src/world/OsmVector).
COPY tiles/Makefile tiles/Makefile
COPY tiles/src/ tiles/src/
COPY src/ src/
RUN make -C tiles build && cp tiles/fb-tiles /usr/local/bin/fb-tiles

FROM docker.io/library/debian:trixie-slim
# nginx-light: proxy_cache is core nginx (ngx_http_proxy_module + the cache machinery), not an
# extras-only module, so the light package is enough. libcurl4 here is the RUNTIME lib fb-tiles
# links against (no -dev headers needed in this stage, nothing gets compiled here). curl the BINARY
# stays for the same reason it always has: the only way to debug an upstream fetch from inside the
# container, and this image is where "why did that tile not arrive" gets asked.
RUN apt-get update && apt-get install -y --no-install-recommends nginx-light curl ca-certificates libcurl4 libsdl3-image0 \
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
