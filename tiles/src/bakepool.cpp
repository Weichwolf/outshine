#include "bakepool.h"
#include "bake.h"
#include "prefetch_api.h"
#include "reply.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FB_BP_SLOTS 512   /* concurrent in-flight DISTINCT cold tiles; bounded above by the general
                            * connection-pool size (main.c) since each in-flight bake ties up exactly
                            * one worker thread -- 512 is headroom, not a tuned limit. */

/* One bake in flight per distinct (kind,z,x,y,TS): the first fb_bakepool_handle() call for a key
 * becomes the owner and runs fb_bake_get() itself, UNLOCKED (this is where real parallelism across
 * different tiles happens -- N owners on N different worker threads bake concurrently). Every call
 * for the SAME key while it's in flight registers as a consumer (refcount++) and blocks on the
 * shared result with no deadline: /bake no longer has a server-side timeout, the client/HTTP
 * connection is the only boundary now. The last consumer to read the result frees the buffer.
 *
 * Residual race, accepted: registration (find-or-alloc + refcount++) happens wherever the caller's
 * worker thread gets to it, and main.c's general pool means several requests for the same tile can
 * sit in the CONNECTION queue before any of them is dispatched. If the owner's bake finishes and
 * self-cleans (every registered waiter already read the result) before a same-tile sibling is even
 * dispatched, that sibling starts a fresh, independent bake instead of joining a finished one --
 * correct, just not deduped against a bake that, from the sibling's perspective, no longer exists.
 * This can only happen when the bake completes faster than connection-queue dispatch latency; real
 * bakes (70-300ms, see bake.c) are far slower than that, so it doesn't occur in practice (verified
 * under real concurrent load) -- only reproducible with an artificial near-instant failure (e.g. a
 * zoom beyond the vector data's native max, rejected in microseconds). Not fixed further: doing so
 * would mean serializing registration through a single thread again, defeating the point of a real
 * worker pool for the one case that never happens with real bake work. */
typedef struct {
    int used, done, ok, refcount;
    fb_albedo_kind k; int z; long x, y, TS;
    uint8_t *body; size_t n;
} fb_bake_slot;

static fb_bake_slot    g_slot[FB_BP_SLOTS];
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv = PTHREAD_COND_INITIALIZER;
static long             g_served = 0, g_full = 0;

/* Both must be called with g_mx held. */
static fb_bake_slot *slot_find(fb_albedo_kind k, int z, long x, long y, int TS) {
    for (int i = 0; i < FB_BP_SLOTS; i++) {
        fb_bake_slot *s = &g_slot[i];
        if (s->used && s->k == k && s->z == z && s->x == x && s->y == y && s->TS == TS) return s;
    }
    return 0;
}
static fb_bake_slot *slot_alloc(fb_albedo_kind k, int z, long x, long y, int TS) {
    for (int i = 0; i < FB_BP_SLOTS; i++) if (!g_slot[i].used) {
        fb_bake_slot *s = &g_slot[i];
        memset(s, 0, sizeof *s);
        s->used = 1; s->k = k; s->z = z; s->x = x; s->y = y; s->TS = TS;
        return s;
    }
    return 0;
}

void fb_bakepool_handle(int fd, fb_albedo_kind k, int z, long x, long y, int TS) {
    pthread_mutex_lock(&g_mx);
    fb_bake_slot *s = slot_find(k, z, x, y, TS);
    int owner = 0;
    if (!s) { s = slot_alloc(k, z, x, y, TS); owner = (s != 0); }

    if (!s) {
        /* all 512 slots hold distinct in-flight cold tiles at once: a genuine capacity emergency,
         * not the normal cold-tile path -- a hard 503, never a 202 (there's no polling contract to
         * fall back to anymore). */
        pthread_mutex_unlock(&g_mx);
        __atomic_fetch_add(&g_full, 1, __ATOMIC_RELAXED);
        fb_reply(fd, "503 Service Unavailable", "text/plain", "bake table full\n");
        return;
    }
    s->refcount++;
    pthread_mutex_unlock(&g_mx);

    if (owner) {
        uint8_t *body = 0; size_t n = 0;
        int ok = fb_bake_get(k, z, x, y, TS, &body, &n);   /* the real work: unlocked, runs in parallel
                                                             * with every other tile's bake */
        pthread_mutex_lock(&g_mx);
        s->ok = ok; s->body = body; s->n = n; s->done = 1;
        pthread_cond_broadcast(&g_cv);
        pthread_mutex_unlock(&g_mx);
    } else {
        pthread_mutex_lock(&g_mx);
        while (!s->done) pthread_cond_wait(&g_cv, &g_mx);
        pthread_mutex_unlock(&g_mx);
    }

    pthread_mutex_lock(&g_mx);
    int ok = s->ok, rz = s->z; long rx = s->x, ry = s->y; int rTS = s->TS;
    uint8_t *body = s->body; size_t n = s->n;
    s->refcount--;
    int last = (s->refcount == 0);
    if (last) s->used = 0;
    pthread_mutex_unlock(&g_mx);

    if (ok) {
        fb_reply_bin(fd, k == FB_ALBEDO_PHOTO ? "image/jpeg" : "image/png", body, n);
        __atomic_fetch_add(&g_served, 1, __ATOMIC_RELAXED);
        fb_pf_warm_bakes(rz, rx, ry, rTS);   /* centre ready: look ahead to the 3x3 neighbours */
    } else {
        fb_reply(fd, "500 Internal Server Error", "text/plain", "bake failed\n");
    }
    if (last) free(body);
}

void fb_bakepool_stats(long *served) {
    if (served) *served = __atomic_load_n(&g_served, __ATOMIC_RELAXED);
}
