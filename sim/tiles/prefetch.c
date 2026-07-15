/* FlightBox tiles — background prefetch: keep OSM and photo warm for the same ground.
 *
 * The queue itself is pure and lives in prefetch.h. This file is the thread and the policy.
 */
#define _GNU_SOURCE
#include "prefetch.h"
#include "prefetch_api.h"
#include "cache.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static fb_pf_queue      g_q;
static pthread_mutex_t  g_mx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cv  = PTHREAD_COND_INITIALIZER;
static int              g_run = 0;
static long             g_done = 0, g_failed = 0;

/* How deep to warm the aerial photo under one vector/terrain tile.
 *
 * The renderer bakes a TS-px texture per tile from (TS/256)^2 photo children at z+log2(TS/256).
 * Its near tier is z14 at 1024 px -> z+2 (4x4); its far tier is z11 at 512 px -> z+1 (2x2).
 * The server cannot know TS, so it warms BOTH depths, plus the tile's own zoom for good measure.
 * That is 1 + 4 + 16 = 21 photo tiles per ground tile — a lot, but each is fetched once, ever,
 * and the whole point is that TAB must not wait on the network. */
#define PF_IMG_DEPTH 2

static void *worker(void *arg){
    (void)arg;
    for(;;){
        fb_pf_job j;
        pthread_mutex_lock(&g_mx);
        while(g_run && !fb_pf_pop(&g_q, &j)) pthread_cond_wait(&g_cv, &g_mx);
        if(!g_run){ pthread_mutex_unlock(&g_mx); break; }
        pthread_mutex_unlock(&g_mx);

        /* fb_cache_get answers from disk if present and fetches upstream otherwise; either way
         * the tile ends up cached, which is the entire purpose. The bytes are not wanted here. */
        uint8_t *b = 0; size_t n = 0;
        if(fb_cache_get((fb_tile_kind)j.kind, j.z, j.x, j.y, &b, &n)){ free(b); g_done++; }
        else g_failed++;
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

static void push_locked(int kind, int z, long x, long y){
    /* Never queue what the source cannot serve: vector stops at z14, terrain z15, imagery z19.
     * Queueing a z16 vector tile would just burn a worker on a guaranteed 404. */
    if(z < 0 || z > fb_src_max_zoom((fb_tile_kind)kind)) return;
    fb_pf_push(&g_q, kind, z, x, y);
}

void fb_pf_warm(fb_tile_kind served, int z, long x, long y){
    if(!g_run) return;
    pthread_mutex_lock(&g_mx);

    /* the sibling ground data for the very same tile */
    if(served != FB_TILE_VECTOR)  push_locked(FB_TILE_VECTOR,  z, x, y);
    if(served != FB_TILE_TERRAIN) push_locked(FB_TILE_TERRAIN, z, x, y);

    /* the photo under it, at the depths the renderer bakes from */
    if(served != FB_TILE_IMAGERY){
        push_locked(FB_TILE_IMAGERY, z, x, y);
        for(int d = 1; d <= PF_IMG_DEPTH; d++){
            int f = 1 << d;
            for(int j = 0; j < f; j++) for(int i = 0; i < f; i++)
                push_locked(FB_TILE_IMAGERY, z + d, x * f + i, y * f + j);
        }
    }
    pthread_cond_signal(&g_cv);
    pthread_mutex_unlock(&g_mx);
}

void fb_pf_stats(long *queued, long *done, long *dropped, long *failed){
    pthread_mutex_lock(&g_mx);
    if(queued)  *queued  = fb_pf_count(&g_q);
    if(done)    *done    = g_done;
    if(dropped) *dropped = g_q.dropped;
    if(failed)  *failed  = g_failed;
    pthread_mutex_unlock(&g_mx);
}
