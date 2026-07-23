#define _GNU_SOURCE
#include "elev.h"
#include "cache.h"
#include "route.h"
#include "prefetch_api.h"
#include "bake.h"
#include "bakepool.h"
#include "stars.h"
#include "lights.h"
#include "tilemap_api.h"
#include "reply.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static void reply_204(int fd) {
    char h[256];
    int n = snprintf(h, sizeof h,
        "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\n"
        "Cache-Control: public, max-age=%ld\r\nConnection: close\r\n\r\n",
        fb_cache_absent_ttl());
    fb_send_all(fd, h, (size_t)n);
}

/* ---- general connection worker pool -------------------------------------------------------------
 * fb-tiles targets a centrally-hosted, thousands-of-clients deployment: the accept loop does the
 * absolute minimum (accept + hand the fd to the pool) and every route -- /bake, /elev, tiles,
 * /health -- runs concurrently on a worker. No keep-alive/HTTP2/TLS ambitions in this C server:
 * that's a reverse proxy's job in front of it; Connection: close per request is fine, the proxy
 * buffers it. Sizing is for BAKE PARALLELISM (dozens of concurrent cold bakes), not C10K socket
 * handling. */
#define FB_CONN_THREADS_DEFAULT 32
#define FB_CONN_THREADS_MAX     256
#define FB_CONN_QCAP            1024

static int              g_cfd[FB_CONN_QCAP];
static int              g_ch = 0, g_ct = 0;
static pthread_mutex_t  g_cmx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_ccv = PTHREAD_COND_INITIALIZER;
static int              g_cthreads = 0;
static long             g_cbusy = 0;

static int cq_count(void) { int c = g_ct - g_ch; return c < 0 ? c + FB_CONN_QCAP : c; }

static void cq_push(int fd) {
    pthread_mutex_lock(&g_cmx);
    /* backpressure, not drop: a full queue means every worker is already busy baking/serving --
     * blocking the (single) accept thread briefly here is correct, self-correcting behavior, not a
     * bug; FB_CONN_QCAP is sized so this is a safety net, not the normal path. */
    while (cq_count() >= FB_CONN_QCAP - 1) pthread_cond_wait(&g_ccv, &g_cmx);
    g_cfd[g_ct] = fd;
    g_ct = (g_ct + 1) % FB_CONN_QCAP;
    pthread_cond_signal(&g_ccv);
    pthread_mutex_unlock(&g_cmx);
}

static int cq_pop(void) {
    pthread_mutex_lock(&g_cmx);
    while (g_ch == g_ct) pthread_cond_wait(&g_ccv, &g_cmx);
    int fd = g_cfd[g_ch];
    g_ch = (g_ch + 1) % FB_CONN_QCAP;
    pthread_cond_signal(&g_ccv);   /* wake an accept thread blocked on a full queue, if any */
    pthread_mutex_unlock(&g_cmx);
    return fd;
}

static void pool_stats(int *threads, long *busy, long *queued) {
    if (threads) *threads = g_cthreads;
    if (busy)    *busy    = __atomic_load_n(&g_cbusy, __ATOMIC_RELAXED);
    if (queued) { pthread_mutex_lock(&g_cmx); *queued = cq_count(); pthread_mutex_unlock(&g_cmx); }
}

static void handle(int fd, char *req) {
    char path[512] = "";
    if (sscanf(req, "GET %511s", path) != 1) { fb_reply(fd, "400 Bad Request", "text/plain", "bad\n"); return; }
    char *qs = strchr(path, '?');
    if (qs) *qs++ = 0;

    if (!strcmp(path, "/elev")) {
        double lat, lon;
        if (!fb_query_double(qs, "lat", &lat) || !fb_query_double(qs, "lon", &lon)) {
            fb_reply(fd, "400 Bad Request", "text/plain", "need lat & lon\n"); return;
        }
        /* ?block=1: wait for the one DEM tile (startup seeds need the real ground on the first
         * call, else they spawn at the wrong elevation). Bare /elev stays instant-503 on a cold
         * tile for the aircraft's AGL poller, which must get an answer fast every 250ms. */
        double bk = 0; int block = fb_query_double(qs, "block", &bk) && bk != 0.0;
        double m;
        int ok = block ? fb_elev_at_blocking(lat, lon, &m, 3000) : fb_elev_at(lat, lon, &m);
        if (!ok) {
            fb_reply(fd, "503 Service Unavailable", "text/plain", "no dem\n"); return;
        }
        char body[64]; snprintf(body, sizeof body, "%.2f\n", m);
        fb_reply(fd, "200 OK", "text/plain", body);
        return;
    }

    if (!strncmp(path, "/bake/", 6)) {
        const char *r = path + 6;
        fb_albedo_kind k;
        if (!strncmp(r, "osm/", 4))        { k = FB_ALBEDO_OSM;   r += 4; }
        else if (!strncmp(r, "photo/", 6)) { k = FB_ALBEDO_PHOTO; r += 6; }
        else { fb_reply(fd, "404 Not Found", "text/plain", "no such albedo\n"); return; }
        int z; long x, y;
        if (sscanf(r, "%d/%ld/%ld", &z, &x, &y) != 3) {
            fb_reply(fd, "400 Bad Request", "text/plain", "want /bake/<kind>/<z>/<x>/<y>\n"); return; }
        double t = 0; int TS = fb_query_double(qs, "tex", &t) ? (int)t : 1024;

        /* Blocks THIS worker until the bake is done -- no deadline, no 202; the client/HTTP timeout
         * is the only boundary now. Per-tile dedup lives in bakepool.c. */
        fb_bakepool_handle(fd, k, z, x, y, TS);
        return;
    }

    if (!strncmp(path, "/t/lights/", 10)) {
        int z; long x, y;
        if (sscanf(path + 10, "%d/%ld/%ld", &z, &x, &y) != 3) {
            fb_reply(fd, "400 Bad Request", "text/plain", "want /t/lights/<z>/<x>/<y>\n"); return; }
        if (z < 0) { fb_reply(fd, "400 Bad Request", "text/plain", "bad z\n"); return; }
        long wn = (long)ldexp(1.0, z);
        if (y < 0 || y >= wn) { fb_reply(fd, "400 Bad Request", "text/plain", "bad y\n"); return; }
        x = ((x % wn) + wn) % wn;

        /* Blocks THIS worker until generated -- same as /bake: no deadline, no 202. fb_lights_get
         * checks disk first internally, dedups concurrent cold requests for the same tile (own
         * inflight table, lights.c), and recurses through the z<12 aggregate pyramid, each level
         * deduped the same way. */
        uint8_t *body = 0; size_t n = 0;
        if (!fb_lights_get(z, x, y, &body, &n)) {
            reply_204(fd);   /* underlying vector data absent entirely -- same absent/empty split as /t/vector */
            return;
        }
        fb_reply_bin(fd, "application/octet-stream", body, n);
        free(body);
        return;
    }

    if (!strncmp(path, "/t/stars/", 9)) {
        /* z = magnitude band, x=y=0 (whole sky). Baked, in-memory, never 404 -> plain 200/404. */
        int band; long sx = 0, sy = 0;
        if (sscanf(path + 9, "%d/%ld/%ld", &band, &sx, &sy) < 1 || sx != 0 || sy != 0) {
            fb_reply(fd, "400 Bad Request", "text/plain", "want /t/stars/<band>/0/0\n"); return; }
        const uint8_t *body; size_t n;
        if (!fb_stars_band(band, &body, &n)) {
            fb_reply(fd, "404 Not Found", "text/plain", "no such band\n"); return; }
        fb_reply_bin(fd, "application/octet-stream", body, n);
        return;
    }

    {
        fb_tile_kind k; int z; long x, y;
        if (fb_route_tile(path, &k, &z, &x, &y)) {
            uint8_t *body = 0; size_t n = 0;

            switch (fb_cache_state(k, z, x, y, &body, &n)) {
            case FB_TILE_UNKNOWN:
                fb_pf_fetch(k, z, x, y);
                fb_reply(fd, "202 Accepted", "text/plain", "fetching\n"); return;
            case FB_TILE_ABSENT:
                reply_204(fd); return;
            case FB_TILE_READY:
                break;
            }
            fb_reply_bin(fd, fb_src_content_type(k), body, n);
            free(body);

            fb_pf_warm(k, z, x, y);
            return;
        }
    }
    if (!strcmp(path, "/health")) {
        long h, m, f, ch, cf, cx, pq, pd, pdr, pf, bh, bb, bf, pif, pab;
        long tq, tmh, tmm, tl, td, bnative, bsuper, lbaked, lserved, bpserved;
        int pth, cthreads; long cbusy, cqueued;
        fb_elev_stats(&h, &m, &f); fb_cache_stats(&ch, &cf, &cx);
        fb_pf_stats(&pq, &pd, &pdr, &pf); fb_bake_stats(&bh, &bb, &bf);
        fb_bake_stats2(&bnative, &bsuper);
        fb_pf_pool(&pth, &pif, &pab);
        fb_tm_stats(&tq, &tmh, &tmm, &tl, &td);
        fb_lights_stats(&lbaked, &lserved);
        fb_bakepool_stats(&bpserved);
        pool_stats(&cthreads, &cbusy, &cqueued);
        char body[1300];
        snprintf(body, sizeof body,
                 "ok pool_threads=%d pool_busy=%ld pool_queued=%ld | "
                 "dem_resident_hits=%ld dem_decoded=%ld dem_fail=%ld | "

                 "cache_hits=%ld upstream_fetches=%ld fetch_fail=%ld absent=%ld | "

                 "prefetch_threads=%d queued=%ld done=%ld dropped=%ld failed=%ld absent=%ld inflight_dedup=%ld | "
                 "bake_disk_hits=%ld baked=%ld bake_fail=%ld scanline_refused=%ld style_unknown_kind=%ld "
                 "native_bakes=%ld super_bakes=%ld bake_block_served=%ld | "

                 "tilemap_queries=%ld hits=%ld misses=%ld learned=%ld dropped=%ld | "
                 "lights_baked=%ld lights_served=%ld\n",
                 cthreads, cbusy, cqueued,
                 h, m, f, ch, cf, cx, fb_cache_absent(), pth, pq, pd, pdr, pf, pab, pif, bh, bb, bf,
                 fb_raster_scanline_overflows(), fb_style_unknown_count(), bnative, bsuper, bpserved,
                 tq, tmh, tmm, tl, td, lbaked, lserved);
        fb_reply(fd, "200 OK", "text/plain", body);
        return;
    }
    fb_reply(fd, "404 Not Found", "text/plain", "no such route\n");
}

static void *conn_worker(void *arg) {
    (void)arg;
    for (;;) {
        int fd = cq_pop();
        __atomic_fetch_add(&g_cbusy, 1, __ATOMIC_RELAXED);
        char req[2048];
        ssize_t n = recv(fd, req, sizeof req - 1, 0);
        if (n > 0) { req[n] = 0; handle(fd, req); }
        close(fd);
        __atomic_fetch_add(&g_cbusy, -1, __ATOMIC_RELAXED);
    }
    return 0;
}

int main(void) {
    int port = getenv("TILES_PORT") ? atoi(getenv("TILES_PORT")) : 8081;
    const char *cache = getenv("TILES_CACHE") ? getenv("TILES_CACHE") : "/var/cache/fbtiles";
    fb_elev_init(cache);
    fb_bake_init(cache);
    fb_lights_init(cache);
    fb_stars_init(getenv("STARS_DIR") ? getenv("STARS_DIR") : "/usr/local/share/fb-stars");
    fb_pf_start();

    int want = getenv("TILES_THREADS") ? atoi(getenv("TILES_THREADS")) : FB_CONN_THREADS_DEFAULT;
    if (want < 1) want = 1;
    if (want > FB_CONN_THREADS_MAX) want = FB_CONN_THREADS_MAX;
    for (int i = 0; i < want; i++) {
        pthread_t th;
        if (pthread_create(&th, 0, conn_worker, 0) != 0) break;
        pthread_detach(th);
        g_cthreads++;
    }
    if (g_cthreads == 0) { fprintf(stderr, "[fb-tiles] no worker thread -- fatal\n"); return 1; }

    /* TILES_BIND: unset (default) keeps every existing local/dev invocation identical (0.0.0.0). The
     * container sets it to 127.0.0.1 -- fb-tiles is the origin BEHIND nginx's proxy_cache there, and
     * shouldn't be reachable except via loopback, independent of whatever port(s) happen to be
     * published on the container. */
    const char *bindaddr = getenv("TILES_BIND");
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_port = htons(port);
    if (bindaddr && *bindaddr) {
        if (inet_pton(AF_INET, bindaddr, &a.sin_addr) != 1) { fprintf(stderr, "[fb-tiles] bad TILES_BIND\n"); return 1; }
    } else {
        a.sin_addr.s_addr = INADDR_ANY;
    }
    if (bind(lfd, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind"); return 1; }
    listen(lfd, 256);
    fprintf(stderr, "[fb-tiles] %s:%d  cache=%s  pool=%d threads  "
                    "(/elev?lat=&lon= , /t/{terrain|vector|imagery}/z/x/y , /t/lights/z/x/y , "
                    "/t/stars/{band}/0/0 , /health)\n", bindaddr && *bindaddr ? bindaddr : "0.0.0.0",
                    port, cache, g_cthreads);

    for (;;) {
        int c = accept(lfd, 0, 0);
        if (c < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        cq_push(c);   /* the accept loop's only job: hand off and go back to accept() */
    }
    return 0;
}
