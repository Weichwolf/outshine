#define _GNU_SOURCE
#include "terrain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

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
