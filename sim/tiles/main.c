/* FlightBox — fb-tiles: the world-data service.
 *
 * One job: dynamically obtain and PREPARE real-world data, so that both consumers can just ask
 * for what they need instead of shipping a preloaded region:
 *
 *   - the ENGINE (fb-aircraft's FDM) needs the ground height under the aircraft, or it cannot
 *     know its true AGL. It asks /elev and gets one number back — we do the tile fetch,
 *     the Terrarium decode and the interpolation.
 *   - the RENDERER (the WASM command center) fetches terrain/vector/imagery tiles here,
 *     prepared for it: we fetch with curl --compressed, so VersaTiles' gzip transfer-encoding
 *     is already undone and the cache holds exactly the raw protobuf osmmesh's decoder wants
 *     (verified: 12 Shortbread layers decode straight from the cached bytes).
 *
 * Everything is cached on disk, so upstream is hit once per tile, ever.
 *
 * Routes:
 *   GET /elev?lat=<f>&lon=<f>       -> "<metres>\n"   ground elevation AMSL (the engine)
 *   GET /t/<kind>/<z>/<x>/<y>       -> raw tile      kind = terrain|vector|imagery (the renderer)
 *   GET /health                     -> cache stats
 *
 * Env: TILES_PORT (8081), TILES_CACHE (/var/cache/fbtiles)
 */
#define _GNU_SOURCE
#include "elev.h"
#include "cache.h"
#include "route.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len) {
        ssize_t w = send(fd, p, len, MSG_NOSIGNAL);
        if (w > 0) { p += w; len -= (size_t)w; continue; }
        if (w < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        return -1;
    }
    return 0;
}

static void reply(int fd, const char *status, const char *ctype, const char *body) {
    char hdr[512];
    int n = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        status, ctype, strlen(body));
    send_all(fd, hdr, (size_t)n);
    send_all(fd, body, strlen(body));
}

/* Binary reply. Tiles are immutable for a given z/x/y, so let every layer cache them hard. */
static void reply_bin(int fd, const char *ctype, const uint8_t *body, size_t n) {
    char hdr[512];
    int h = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nCache-Control: public, max-age=31536000, immutable\r\n"
        "Connection: close\r\n\r\n", ctype, n);
    send_all(fd, hdr, (size_t)h);
    send_all(fd, body, n);
}

static void handle(int fd, char *req) {
    char path[512] = "";
    if (sscanf(req, "GET %511s", path) != 1) { reply(fd, "400 Bad Request", "text/plain", "bad\n"); return; }
    char *qs = strchr(path, '?');
    if (qs) *qs++ = 0;

    if (!strcmp(path, "/elev")) {
        double lat, lon;
        if (!fb_query_double(qs, "lat", &lat) || !fb_query_double(qs, "lon", &lon)) {
            reply(fd, "400 Bad Request", "text/plain", "need lat & lon\n"); return;
        }
        double m;
        if (!fb_elev_at(lat, lon, &m)) {
            /* Honest failure: the caller must keep its previous value, not treat this as
             * "the ground is at 0". */
            reply(fd, "503 Service Unavailable", "text/plain", "no dem\n"); return;
        }
        char body[64]; snprintf(body, sizeof body, "%.2f\n", m);
        reply(fd, "200 OK", "text/plain", body);
        return;
    }
    /* /t/<kind>/<z>/<x>/<y> — raw tiles for the renderer (terrain, vector, imagery). */
    {
        fb_tile_kind k; int z; long x, y;
        if (fb_route_tile(path, &k, &z, &x, &y)) {
            uint8_t *body = 0; size_t n = 0;
            if (!fb_cache_get(k, z, x, y, &body, &n)) {
                /* Upstream has genuine holes (ocean, no imagery); say so honestly and let the
                 * renderer skip the tile rather than draw whatever a 200 with junk would give. */
                reply(fd, "404 Not Found", "text/plain", "no tile\n"); return;
            }
            reply_bin(fd, fb_src_content_type(k), body, n);
            free(body);
            return;
        }
    }
    if (!strcmp(path, "/health")) {
        long h, m, f, ch, cf, cx; fb_elev_stats(&h, &m, &f); fb_cache_stats(&ch, &cf, &cx);
        char body[256];
        snprintf(body, sizeof body,
                 "ok dem_resident_hits=%ld dem_decoded=%ld dem_fail=%ld | "
                 "cache_hits=%ld upstream_fetches=%ld fetch_fail=%ld\n", h, m, f, ch, cf, cx);
        reply(fd, "200 OK", "text/plain", body);
        return;
    }
    reply(fd, "404 Not Found", "text/plain", "no such route\n");
}

int main(void) {
    int port = getenv("TILES_PORT") ? atoi(getenv("TILES_PORT")) : 8081;
    const char *cache = getenv("TILES_CACHE") ? getenv("TILES_CACHE") : "/var/cache/fbtiles";
    fb_elev_init(cache);

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(port);
    if (bind(lfd, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind"); return 1; }
    listen(lfd, 16);
    fprintf(stderr, "[fb-tiles] :%d  cache=%s  (/elev?lat=&lon= , /t/{terrain|vector|imagery}/z/x/y , /health)\n", port, cache);

    for (;;) {
        int c = accept(lfd, 0, 0);
        if (c < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        char req[2048];
        ssize_t n = recv(c, req, sizeof req - 1, 0);
        if (n > 0) { req[n] = 0; handle(c, req); }
        close(c);
    }
    return 0;
}
