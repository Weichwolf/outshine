/* FlightBox tiles — background prefetch: keep OSM and photo warm for the same ground.
 *
 * The queue itself is pure and lives in prefetch.h. This file is the thread and the policy.
 */
#define _GNU_SOURCE
#include "prefetch.h"
#include "prefetch_api.h"
#include "cache.h"
#include "bake.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* 4 is politeness toward upstream, not a measured optimum: fb-tiles scales to 16 and so does
 * upstream, but nobody has measured what VersaTiles/Esri/AWS tolerate and their policies name no
 * limit -- silence is not permission. Raising it trades a measured gain against an unmeasured ban.
 */
#define FB_PF_THREADS_DEFAULT 4
#define FB_PF_THREADS_MAX     64

static fb_pf_queue      g_q;
static pthread_mutex_t  g_mx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cv  = PTHREAD_COND_INITIALIZER;
static int              g_run = 0;
static long             g_done = 0, g_failed = 0, g_absent = 0;
static int              g_nthreads = 0;

/* The queue only de-dups against WAITING jobs; a popped one is invisible to it, so two workers
 * could fetch the same tile at once. Policy, not queue -- prefetch.h stays pure. */
static fb_pf_job g_inflight[FB_PF_THREADS_MAX];
static int       g_inflight_on[FB_PF_THREADS_MAX];
static long      g_inflight_hits = 0;

/* Caller must hold g_mx. */
static int inflight_has(int kind, int z, long x, long y, int tex){
    for(int i = 0; i < g_nthreads; i++)
        if(g_inflight_on[i] && g_inflight[i].kind == kind && g_inflight[i].z == z
           && g_inflight[i].x == x && g_inflight[i].y == y && g_inflight[i].tex == tex) return 1;
    return 0;
}

/* Push, but not if a worker is already on it. Caller must hold g_mx. */
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
        g_inflight[slot] = j; g_inflight_on[slot] = 1;   /* claim it before letting go of the lock */
        pthread_mutex_unlock(&g_mx);

        /* The bytes are not wanted here — only the cache side effect. */
        uint8_t *b = 0; size_t n = 0;
        int ok, absent = 0;
        if(j.kind >= FB_PF_BAKE_OSM)
            ok = fb_bake_get(j.kind==FB_PF_BAKE_PHOTO ? FB_ALBEDO_PHOTO : FB_ALBEDO_OSM,
                             j.z, j.x, j.y, j.tex, &b, &n);
        else {
            ok = fb_cache_get((fb_tile_kind)j.kind, j.z, j.x, j.y, &b, &n);
            /* A tile upstream does not have is not a failure. */
            if(!ok){ uint8_t *d = 0; size_t dn = 0;
                     absent = fb_cache_state((fb_tile_kind)j.kind, j.z, j.x, j.y, &d, &dn) == FB_TILE_ABSENT;
                     free(d); }
        }
        if(ok) free(b);

        pthread_mutex_lock(&g_mx);            /* ++ is read-modify-write; the pool shares these */
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

    /* Held across the whole loop: inflight_has reads g_nthreads while it is still growing. */
    pthread_mutex_lock(&g_mx);
    g_run = 1;
    for(int i = 0; i < want; i++){
        pthread_t th;
        /* counts what STARTED: a slot nobody clears would never de-dup */
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

/* Warm the 3x3 block around a requested albedo: the tile itself (the OTHER albedo of it -- TAB
 * must not wait) and its 8 neighbours in both albedos.
 *
 * The 8 neighbours are the point: the aircraft is moving. Whatever is one tile away is what the
 * renderer asks for next, and a cold photo bake is 1.6 s inside this server's single accept()
 * loop -- 1.6 s in which nobody is served at all. Bake it on the worker before it is asked for,
 * or pay for it in the request that asks.
 *
 * `tex` comes from the request rather than being guessed: the renderer's near tier asks for 1024
 * and its far tier for 512, and baking a size nobody requests is work for its own sake. */
void fb_pf_warm_bakes(int z, long x, long y, int tex){
    if(!g_run || z < 0) return;
    long n = 1L << z;                       /* wrap x at the antimeridian; y has no wrap */
    pthread_mutex_lock(&g_mx);
    for(int dy = -1; dy <= 1; dy++) for(int dx = -1; dx <= 1; dx++){
        long tx = ((x + dx) % n + n) % n, ty = y + dy;
        if(ty < 0 || ty >= n) continue;     /* off the top/bottom of the world */
        push_locked(FB_PF_BAKE_OSM,   z, tx, ty, tex);
        push_locked(FB_PF_BAKE_PHOTO, z, tx, ty, tex);
    }
    /* broadcast: signal wakes ONE, which would drain all 18 jobs through one thread */
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

/* Raw-tile requests no longer trigger anything: the renderer stopped fetching raw tiles when the
 * rasteriser moved here, and the bake pulls its own inputs. Kept as a no-op seam rather than
 * ripped out, because /t/ is still the honest way to get a raw tile. */
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
