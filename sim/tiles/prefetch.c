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

/* How many workers. One was not a decision anybody wrote down -- the file said "the thread", in the
 * singular, and a cold region therefore streamed at the speed of one serial fetch.
 *
 * 4 is MEASURED, not chosen. 64 cold tiles through /t/, each thread count on its own untouched
 * region, ladder walked up and back down (the two passes agree, so the run did not drift):
 *      threads   1      2      4      8     16
 *      drain  4.04s  1.94s  1.40s  1.60s  1.58s
 *      vs 1   1.00x  2.09x  2.89x  2.53x  2.55x
 * It saturates at 4 and 8/16 buy nothing. What this does NOT say is that 4 beats 8: that gap
 * (0.20 s) is inside 4's own spread (1.48/1.31), so the honest claim is "1->2->4 helps, past 4 is
 * flat" -- and 4 is the cheapest point on the flat part. The ceiling is ~2.9x rather than linear
 * and WHY is unmeasured; do not invent a reason for it here.
 *
 * Overridable because the right number depends on the box and on what upstream tolerates, and a
 * value nobody can change is a value nobody can re-measure when either changes. */
#define FB_PF_THREADS_DEFAULT 4
#define FB_PF_THREADS_MAX     64

static fb_pf_queue      g_q;
static pthread_mutex_t  g_mx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cv  = PTHREAD_COND_INITIALIZER;
static int              g_run = 0;
static long             g_done = 0, g_failed = 0, g_absent = 0;
static int              g_nthreads = 0;

/* IN-FLIGHT set. The queue de-duplicates against jobs that are WAITING; a job that has been popped
 * is invisible to it. With one worker that was harmless -- the duplicate simply found the tile on
 * disk a moment later and cost a cheap hit. With a pool it is not: two workers pop the same tile's
 * two copies and fetch it from upstream TWICE, in parallel, for nothing.
 *
 * So the popped job stays visible until it is finished. This lives here and not in prefetch.h
 * because it is policy, not queue: the header stays pure and assertable without a network. */
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

        /* Either call answers from disk if present and does the expensive work otherwise; either
         * way the result ends up cached, which is the entire purpose. The bytes are not wanted
         * here — only the side effect. */
        uint8_t *b = 0; size_t n = 0;
        int ok, absent = 0;
        if(j.kind >= FB_PF_BAKE_OSM)
            ok = fb_bake_get(j.kind==FB_PF_BAKE_PHOTO ? FB_ALBEDO_PHOTO : FB_ALBEDO_OSM,
                             j.z, j.x, j.y, j.tex, &b, &n);
        else {
            ok = fb_cache_get((fb_tile_kind)j.kind, j.z, j.x, j.y, &b, &n);
            /* A tile upstream does not HAVE is not a failure, and counting it as one is the very
             * mistake this pool now exists to stop making one layer down. Over an ocean region
             * `failed` would climb into the hundreds and read as "the fetcher is broken" -- a
             * counter lying in the exact way that costs an afternoon of debugging. */
            if(!ok){ uint8_t *d = 0; size_t dn = 0;
                     absent = fb_cache_state((fb_tile_kind)j.kind, j.z, j.x, j.y, &d, &dn) == FB_TILE_ABSENT;
                     free(d); }
        }
        if(ok) free(b);

        /* Both counters are shared now. They were incremented outside the lock, which was fine for
         * exactly as long as there was one thread: `g_done++` is read-modify-write, so a pool turns
         * the numbers we would judge the pool BY into a data race. A benchmark that corrupts its
         * own counters reports whatever it likes. */
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

    /* The lock is held across the whole spawn loop on purpose. g_nthreads is read by inflight_has
     * (under the lock) and written here; without this, an early worker reads it while it is still
     * growing -- a data race, and one that would only ever show up as a missed de-duplication,
     * i.e. as "the network is slow today". Workers block on their first pop until this returns. */
    pthread_mutex_lock(&g_mx);
    g_run = 1;
    for(int i = 0; i < want; i++){
        pthread_t th;
        /* g_nthreads counts what STARTED, not what was asked for: inflight_has scans it, and a
         * thread that failed to spawn must not leave a slot nobody clears. */
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
    /* broadcast, not signal: this pushed up to 18 jobs and there are now N workers asleep. A signal
     * wakes ONE, which would drain 18 jobs through one thread and quietly undo the pool. */
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
