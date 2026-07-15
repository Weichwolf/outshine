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

static fb_pf_queue      g_q;
static pthread_mutex_t  g_mx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cv  = PTHREAD_COND_INITIALIZER;
static int              g_run = 0;
static long             g_done = 0, g_failed = 0;



static void *worker(void *arg){
    (void)arg;
    for(;;){
        fb_pf_job j;
        pthread_mutex_lock(&g_mx);
        while(g_run && !fb_pf_pop(&g_q, &j)) pthread_cond_wait(&g_cv, &g_mx);
        if(!g_run){ pthread_mutex_unlock(&g_mx); break; }
        pthread_mutex_unlock(&g_mx);

        /* Either call answers from disk if present and does the expensive work otherwise; either
         * way the result ends up cached, which is the entire purpose. The bytes are not wanted
         * here — only the side effect. */
        uint8_t *b = 0; size_t n = 0;
        int ok;
        if(j.kind >= FB_PF_BAKE_OSM)
            ok = fb_bake_get(j.kind==FB_PF_BAKE_PHOTO ? FB_ALBEDO_PHOTO : FB_ALBEDO_OSM,
                             j.z, j.x, j.y, j.tex, &b, &n);
        else
            ok = fb_cache_get((fb_tile_kind)j.kind, j.z, j.x, j.y, &b, &n);
        if(ok){ free(b); g_done++; } else g_failed++;
    }
    return 0;
}

void fb_pf_start(void){
    fb_pf_init(&g_q);
    g_run = 1;
    pthread_t th;
    if(pthread_create(&th, 0, worker, 0) == 0) pthread_detach(th);
    else { g_run = 0; fprintf(stderr, "[pf] no thread — prefetch disabled\n"); }
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
        fb_pf_push(&g_q, FB_PF_BAKE_OSM,   z, tx, ty, tex);
        fb_pf_push(&g_q, FB_PF_BAKE_PHOTO, z, tx, ty, tex);
    }
    pthread_cond_signal(&g_cv);
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
