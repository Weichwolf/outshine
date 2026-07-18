#define _GNU_SOURCE
#include "terrain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define FB_POLL_MS 250

static char   g_addr[128] = "fb-tiles:8081";
static double g_ground = 0.0;                 /* latest ground elevation, m AMSL */
static double g_lat = 0.0, g_lon = 0.0;
static int    g_have_pos = 0;
static long   g_ok = 0, g_fail = 0;
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;

/* One /elev query. Uses curl rather than a hand-rolled HTTP client: the aircraft already
 * shells out to curl for live weather, and this is off the real-time path. */
int fb_terrain_lookup(const char *addr, double lat, double lon, double *out) {
    char cmd[512];
    snprintf(cmd, sizeof cmd,
             "curl -s -f --max-time 5 'http://%s/elev?lat=%.6f&lon=%.6f'", addr, lat, lon);
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    int rc = pclose(f);
    if (rc != 0 || n == 0) return 0;
    char *end = 0;
    double v = strtod(buf, &end);
    if (end == buf) return 0;                 /* not a number -> treat as failure */
    *out = v;
    return 1;
}

/* Start-up: get home's TRUE ground before the sim spawns, or the aircraft launches at the wrong
 * elevation (a cold origin used to seed 71 m and fly 1200 m underground until the AGL clamp
 * snapped it to the ground at 0 AGL, losing the 2 m launch height). Retries because two things
 * fail at boot: (a) the DEM tile is cold -- ?block=1 makes fb-tiles wait for the single tile, but
 * we still retry if that wait times out; (b) fb-tiles may not be listening yet (podman guarantees
 * the container STARTED, not that it is ready), so an early curl gets connection-refused. Bounded
 * by a wall-clock deadline; only if the whole deadline passes do we give up and let the caller
 * seed. Uses the same curl-shell primitive as the rest of this client. */
int fb_terrain_lookup_deadline(const char *addr, double lat, double lon, double *out, double deadline_s) {
    char cmd[512];
    snprintf(cmd, sizeof cmd,
             "curl -s --max-time 6 -w '\\n%%{http_code}' "
             "'http://%s/elev?lat=%.6f&lon=%.6f&block=1'", addr, lat, lon);
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    int connected = 0;                    /* have we ever reached the server (any HTTP reply)? */
    const double connect_grace = 3.0;     /* no server within this -> seed & run; do NOT wait out the
                                           * full deadline (offline dev / the E2E harness runs no tiles,
                                           * and the boot race is over in ~1 s). Only a REACHED server
                                           * returning 503 (cold tile) earns the long deadline. */
    for (;;) {
        FILE *f = popen(cmd, "r");
        char buf[128] = {0}; size_t n = 0; int rc = -1;
        if (f) { n = fread(buf, 1, sizeof buf - 1, f); rc = pclose(f); }
        if (rc == 0) {                    /* curl got an HTTP reply (200 or 503): the server is up */
            connected = 1;
            if (n > 0) {
                char *nl = strrchr(buf, '\n');            /* curl -w appends "\n<http_code>" */
                int code = nl ? atoi(nl + 1) : 0;
                if (nl) *nl = 0;
                char *end = 0; double v = strtod(buf, &end);
                if (code == 200 && end != buf) { *out = v; return 1; }
            }
        }
        struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
        double el = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        if (!connected && el >= connect_grace) return 0;  /* server absent: seed, don't block the sim */
        if (el >= deadline_s) return 0;                   /* reached but tile never came: give up */
        usleep(300 * 1000);
    }
}

static void *poller(void *arg) {
    (void)arg;
    for (;;) {
        double lat, lon; int have;
        pthread_mutex_lock(&g_mx); lat = g_lat; lon = g_lon; have = g_have_pos; pthread_mutex_unlock(&g_mx);
        if (have) {
            double m;
            if (fb_terrain_lookup(g_addr, lat, lon, &m)) {
                pthread_mutex_lock(&g_mx); g_ground = m; g_ok++; pthread_mutex_unlock(&g_mx);
            } else {
                pthread_mutex_lock(&g_mx); g_fail++; pthread_mutex_unlock(&g_mx);
                /* keep the last good value on purpose — see terrain.h */
            }
        }
        usleep(FB_POLL_MS * 1000);
    }
    return 0;
}

int fb_terrain_start(const char *addr, double seed_elev) {
    if (addr && *addr) snprintf(g_addr, sizeof g_addr, "%s", addr);
    g_ground = seed_elev;
    pthread_t th;
    if (pthread_create(&th, NULL, poller, NULL) != 0) return -1;
    pthread_detach(th);
    return 0;
}

void fb_terrain_set_pos(double lat, double lon) {
    pthread_mutex_lock(&g_mx); g_lat = lat; g_lon = lon; g_have_pos = 1; pthread_mutex_unlock(&g_mx);
}

double fb_terrain_ground(void) {
    pthread_mutex_lock(&g_mx); double v = g_ground; pthread_mutex_unlock(&g_mx);
    return v;
}

void fb_terrain_stats(long *ok, long *fail) {
    pthread_mutex_lock(&g_mx);
    if (ok) *ok = g_ok;
    if (fail) *fail = g_fail;
    pthread_mutex_unlock(&g_mx);
}
