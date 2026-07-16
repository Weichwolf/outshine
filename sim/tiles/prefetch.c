#define _GNU_SOURCE
#include "prefetch.h"
#include "prefetch_api.h"
#include "cache.h"
#include "bake.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define FB_PF_THREADS_DEFAULT 4
#define FB_PF_THREADS_MAX     64

static fb_pf_queue      g_q;
static pthread_mutex_t  g_mx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cv  = PTHREAD_COND_INITIALIZER;
static int              g_run = 0;
static long             g_done = 0, g_failed = 0, g_absent = 0;
static int              g_nthreads = 0;

static fb_pf_job g_inflight[FB_PF_THREADS_MAX];
static int       g_inflight_on[FB_PF_THREADS_MAX];
static long      g_inflight_hits = 0;

static int inflight_has(int kind, int z, long x, long y, int tex){
    for(int i = 0; i < g_nthreads; i++)
        if(g_inflight_on[i] && g_inflight[i].kind == kind && g_inflight[i].z == z
           && g_inflight[i].x == x && g_inflight[i].y == y && g_inflight[i].tex == tex) return 1;
    return 0;
}

static void push_locked(int kind, int z, long x, long y, int tex){
    if(inflight_has(kind, z, x, y, tex)){ g_inflight_hits++; return; }
    fb_pf_push(&g_q, kind, z, x, y, tex);
}

static void *worker(void *arg){
    int slot = (int)(long)arg;
    for(;;){
        fb_pf_job j;
        pthread_mutex_lock(&g_mx);
        while(g_run && !fb_pf_pop(&g_q, &j)) pthread_cond_wait(&g_cv, &g_mx);
        if(!g_run){ pthread_mutex_unlock(&g_mx); break; }
        g_inflight[slot] = j; g_inflight_on[slot] = 1;
        pthread_mutex_unlock(&g_mx);

        uint8_t *b = 0; size_t n = 0;
        int ok, absent = 0;
        if(j.kind >= FB_PF_BAKE_OSM)
            ok = fb_bake_get(j.kind==FB_PF_BAKE_PHOTO ? FB_ALBEDO_PHOTO : FB_ALBEDO_OSM,
                             j.z, j.x, j.y, j.tex, &b, &n);
        else {
            ok = fb_cache_get((fb_tile_kind)j.kind, j.z, j.x, j.y, &b, &n);

            if(!ok){ uint8_t *d = 0; size_t dn = 0;
                     absent = fb_cache_state((fb_tile_kind)j.kind, j.z, j.x, j.y, &d, &dn) == FB_TILE_ABSENT;
                     free(d); }
        }
        if(ok) free(b);

        pthread_mutex_lock(&g_mx);
        if(ok) g_done++; else if(absent) g_absent++; else g_failed++;
        g_inflight_on[slot] = 0;
        pthread_mutex_unlock(&g_mx);
    }
    return 0;
}

void fb_pf_start(void){
    fb_pf_init(&g_q);
    const char *e = getenv("TILES_PF_THREADS");
    int want = e ? atoi(e) : FB_PF_THREADS_DEFAULT;
    if(want < 1) want = 1;
    if(want > FB_PF_THREADS_MAX) want = FB_PF_THREADS_MAX;

    pthread_mutex_lock(&g_mx);
    g_run = 1;
    for(int i = 0; i < want; i++){
        pthread_t th;

        if(pthread_create(&th, 0, worker, (void *)(long)g_nthreads) != 0) break;
        pthread_detach(th);
        g_nthreads++;
    }
    int started = g_nthreads;
    if(started == 0) g_run = 0;
    pthread_mutex_unlock(&g_mx);
    if(started == 0){ fprintf(stderr, "[pf] no thread — prefetch disabled\n"); return; }
    fprintf(stderr, "[pf] %d worker thread%s\n", g_nthreads, g_nthreads == 1 ? "" : "s");
}

void fb_pf_warm_bakes(int z, long x, long y, int tex){
    if(!g_run || z < 0) return;
    long n = 1L << z;
    pthread_mutex_lock(&g_mx);
    for(int dy = -1; dy <= 1; dy++) for(int dx = -1; dx <= 1; dx++){
        long tx = ((x + dx) % n + n) % n, ty = y + dy;
        if(ty < 0 || ty >= n) continue;
        push_locked(FB_PF_BAKE_OSM,   z, tx, ty, tex);
        push_locked(FB_PF_BAKE_PHOTO, z, tx, ty, tex);
    }

    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mx);
}

void fb_pf_fetch(fb_tile_kind k, int z, long x, long y){
    if(!g_run || z < 0 || z > fb_src_max_zoom(k)) return;
    pthread_mutex_lock(&g_mx);
    fb_pf_push(&g_q, (int)k, z, x, y, 0);
    pthread_cond_signal(&g_cv);
    pthread_mutex_unlock(&g_mx);
}

void fb_pf_warm(fb_tile_kind served, int z, long x, long y){
    (void)served; (void)z; (void)x; (void)y;
}

void fb_pf_stats(long *queued, long *done, long *dropped, long *failed){
    pthread_mutex_lock(&g_mx);
    if(queued)  *queued  = fb_pf_count(&g_q);
    if(done)    *done    = g_done;
    if(dropped) *dropped = g_q.dropped;
    if(failed)  *failed  = g_failed;
    pthread_mutex_unlock(&g_mx);
}

void fb_pf_pool(int *threads, long *inflight_hits, long *absent){
    pthread_mutex_lock(&g_mx);
    if(threads)       *threads       = g_nthreads;
    if(inflight_hits) *inflight_hits = g_inflight_hits;
    if(absent)        *absent        = g_absent;
    pthread_mutex_unlock(&g_mx);
}
