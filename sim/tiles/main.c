#define _GNU_SOURCE
#include "elev.h"
#include "cache.h"
#include "route.h"
#include "prefetch_api.h"
#include "bake.h"
#include "tilemap_api.h"
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

static void reply_204(int fd) {
    char h[256];
    int n = snprintf(h, sizeof h,
        "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\n"
        "Cache-Control: public, max-age=%ld\r\nConnection: close\r\n\r\n",
        fb_cache_absent_ttl());
    send_all(fd, h, (size_t)n);
}

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

            reply(fd, "503 Service Unavailable", "text/plain", "no dem\n"); return;
        }
        char body[64]; snprintf(body, sizeof body, "%.2f\n", m);
        reply(fd, "200 OK", "text/plain", body);
        return;
    }

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

        if (!fb_bake_ondisk(k, z, x, y, TS, &body, &n)) {
            fb_pf_warm_bakes(z, x, y, TS);
            reply(fd, "202 Accepted", "text/plain", "baking\n");
            return;
        }
        reply_bin(fd, k == FB_ALBEDO_PHOTO ? "image/jpeg" : "image/png", body, n);
        free(body);

        fb_pf_warm_bakes(z, x, y, TS);
        return;
    }

    {
        fb_tile_kind k; int z; long x, y;
        if (fb_route_tile(path, &k, &z, &x, &y)) {
            uint8_t *body = 0; size_t n = 0;

            switch (fb_cache_state(k, z, x, y, &body, &n)) {
            case FB_TILE_UNKNOWN:
                fb_pf_fetch(k, z, x, y);
                reply(fd, "202 Accepted", "text/plain", "fetching\n"); return;
            case FB_TILE_ABSENT:
                reply_204(fd); return;
            case FB_TILE_READY:
                break;
            }
            reply_bin(fd, fb_src_content_type(k), body, n);
            free(body);

            fb_pf_warm(k, z, x, y);
            return;
        }
    }
    if (!strcmp(path, "/health")) {
        long h, m, f, ch, cf, cx, pq, pd, pdr, pf, bh, bb, bf, pif, pab;
        long tq, tmh, tmm, tl, td;
        int pth;
        fb_elev_stats(&h, &m, &f); fb_cache_stats(&ch, &cf, &cx);
        fb_pf_stats(&pq, &pd, &pdr, &pf); fb_bake_stats(&bh, &bb, &bf);
        fb_pf_pool(&pth, &pif, &pab);
        fb_tm_stats(&tq, &tmh, &tmm, &tl, &td);
        char body[900];
        snprintf(body, sizeof body,
                 "ok dem_resident_hits=%ld dem_decoded=%ld dem_fail=%ld | "

                 "cache_hits=%ld upstream_fetches=%ld fetch_fail=%ld absent=%ld | "

                 "prefetch_threads=%d queued=%ld done=%ld dropped=%ld failed=%ld absent=%ld inflight_dedup=%ld | "
                 "bake_disk_hits=%ld baked=%ld bake_fail=%ld scanline_refused=%ld | "

                 "tilemap_queries=%ld hits=%ld misses=%ld learned=%ld dropped=%ld\n",
                 h, m, f, ch, cf, cx, fb_cache_absent(), pth, pq, pd, pdr, pf, pab, pif, bh, bb, bf,
                 fb_raster_scanline_overflows(), tq, tmh, tmm, tl, td);
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
    fb_pf_start();

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
