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
#include "prefetch_api.h"
#include "bake.h"
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
    /* /bake/<osm|photo>/<z>/<x>/<y>[?tex=N] — the ground ALBEDO, ready to upload.
     *
     * This is the "prepares" half of this service's job, which used to be a lie: we handed out
     * raw vector tiles and the browser rasterised them again on every load. The albedo is
     * view-independent -- it is what the ground IS, not how it is lit -- so it is computed once
     * and kept. Lighting is NOT baked and never will be: the renderer applies our own sun
     * per-pixel from the DEM normals. Baking an albedo is not baking light. */
    if (!strncmp(path, "/bake/", 6)) {
        const char *r = path + 6;
        fb_albedo_kind k;
        if (!strncmp(r, "osm/", 4))        { k = FB_ALBEDO_OSM;   r += 4; }
        else if (!strncmp(r, "photo/", 6)) { k = FB_ALBEDO_PHOTO; r += 6; }
        else { reply(fd, "404 Not Found", "text/plain", "no such albedo\n"); return; }
        int z; long x, y;
        if (sscanf(r, "%d/%ld/%ld", &z, &x, &y) != 3) {
            reply(fd, "400 Bad Request", "text/plain", "want /bake/<kind>/<z>/<x>/<y>\n"); return; }
        double t = 0; int TS = fb_query_double(qs, "tex", &t) ? (int)t : 1024;
        uint8_t *body = 0; size_t n = 0;

        /* ON DISK ONLY. Baking here would block the whole server: this is a single accept() loop
         * and a cold photo bake measures 1.6 s -- 1.6 s in which nobody is served, times ~25
         * tiles for a fresh region. So a miss queues the work and says so immediately; the
         * renderer draws a placeholder and asks again. Nothing anywhere waits on a bake.
         *
         * 202, NOT 404 -- and that is not cosmetics, it is the difference between a world that
         * loads and one that does not. This said 404, which also means "there is no such thing",
         * and the browser believed the second reading: tilesrc_js.h cached every 404 as a
         * permanent hole and never asked again. Measured over the Matterhorn (cold region, same
         * build): 0 chunks drawn, 39 pending, then silence -- while this server was already
         * answering 200 for the very same tiles. Hameln only ever worked because its cache volume
         * has been warm for months, which is also why every number we ever took was a warm one.
         * 202 Accepted says what is true and nothing else: the work is queued, ask again. */
        if (!fb_bake_ondisk(k, z, x, y, TS, &body, &n)) {
            fb_pf_warm_bakes(z, x, y, TS);          /* this tile + its 8 neighbours, both albedos */
            reply(fd, "202 Accepted", "text/plain", "baking\n");
            return;
        }
        reply_bin(fd, k == FB_ALBEDO_PHOTO ? "image/jpeg" : "image/png", body, n);
        free(body);
        /* Answer first, warm after: the aircraft is moving, so the 8 neighbours are what gets
         * asked for next. Bake them on the worker before the request for them arrives. */
        fb_pf_warm_bakes(z, x, y, TS);
        return;
    }

    /* /t/<kind>/<z>/<x>/<y> — raw tiles for the renderer (terrain, vector, imagery). */
    {
        fb_tile_kind k; int z; long x, y;
        if (fb_route_tile(path, &k, &z, &x, &y)) {
            uint8_t *body = 0; size_t n = 0;
            /* Disk only. fb_cache_get would curl on a miss -- up to 20 s inside this single
             * accept() loop, i.e. 20 s in which nobody is served, including the live flight view.
             * A miss queues the fetch and says so; the caller asks again. Nothing here ever waits
             * on the network.
             *
             * The old comment here argued that 404 was "the honest answer" for genuine holes
             * (ocean, no imagery) as well as for a queued fetch. That is exactly the bug: one
             * status for two facts, so the caller cannot act on either. 202 = "queued, ask again"
             * is now the only thing this line claims.
             *
             * The genuine hole has NO representation yet, and pretending otherwise would be the
             * same mistake again: this server never learns that upstream lacks a tile -- there is
             * no negative caching anywhere in prefetch/tilesrc/bake. So an ocean tile is retried
             * forever until that exists (then: 204 No Content). A retry loop is noisy and shows
             * up; a silent permanent hole looks exactly like a working world. We take the noisy
             * one on purpose. */
            if (!fb_cache_ondisk(k, z, x, y, &body, &n)) {
                fb_pf_fetch(k, z, x, y);
                reply(fd, "202 Accepted", "text/plain", "fetching\n"); return;
            }
            reply_bin(fd, fb_src_content_type(k), body, n);
            free(body);
            /* Warm the OTHER kinds for the same ground, in the background. The renderer can
             * switch its ground between the OSM render and the aerial photo at a keypress —
             * meant as a fallback for when the camera cannot deliver — and a fallback view that
             * first downloads 400 tiles is not a fallback. Answer first, warm after: this must
             * never make the request the renderer IS waiting for any slower. */
            fb_pf_warm(k, z, x, y);
            return;
        }
    }
    if (!strcmp(path, "/health")) {
        long h, m, f, ch, cf, cx, pq, pd, pdr, pf, bh, bb, bf, pif;
        int pth;
        fb_elev_stats(&h, &m, &f); fb_cache_stats(&ch, &cf, &cx);
        fb_pf_stats(&pq, &pd, &pdr, &pf); fb_bake_stats(&bh, &bb, &bf);
        fb_pf_pool(&pth, &pif);
        char body[640];
        snprintf(body, sizeof body,
                 "ok dem_resident_hits=%ld dem_decoded=%ld dem_fail=%ld | "
                 /* absent = upstream said 404. Split out of fetch_fail because "there is no such
                  * tile" and "the network broke" are different facts, and only one is worth a
                  * retry. Still only a counter: nothing acts on it yet. */
                 "cache_hits=%ld upstream_fetches=%ld fetch_fail=%ld absent=%ld | "
                 /* threads = what STARTED, not what TILES_PF_THREADS asked for. */
                 "prefetch_threads=%d queued=%ld done=%ld dropped=%ld failed=%ld inflight_dedup=%ld | "
                 "bake_disk_hits=%ld baked=%ld bake_fail=%ld scanline_refused=%ld\n",
                 h, m, f, ch, cf, cx, fb_cache_absent(), pth, pq, pd, pdr, pf, pif, bh, bb, bf,
                 fb_raster_scanline_overflows());
        reply(fd, "200 OK", "text/plain", body);
        return;
    }
    reply(fd, "404 Not Found", "text/plain", "no such route\n");
}

int main(void) {
    int port = getenv("TILES_PORT") ? atoi(getenv("TILES_PORT")) : 8081;
    const char *cache = getenv("TILES_CACHE") ? getenv("TILES_CACHE") : "/var/cache/fbtiles";
    fb_elev_init(cache);
    fb_bake_init(cache);
    fb_pf_start();   /* background warming of the sibling tile kinds */

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
