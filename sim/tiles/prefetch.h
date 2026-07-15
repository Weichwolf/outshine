/* FlightBox tiles — the prefetch work queue (pure).
 *
 * Why prefetching exists: the renderer can switch the ground between the OSM render and the
 * aerial photo (TAB). That switch is meant to be a fallback for when the camera cannot deliver —
 * lost signal, dead sensor, darkness, glare. A fallback view that first has to download 400 tiles
 * is not a fallback. So whenever we serve one kind of tile for an area, we warm the others.
 *
 * Why a queue rather than just fetching inline: the server is a single accept() loop. Fetching
 * 20 extra tiles before answering would make every request pay for tiles nobody asked for, and
 * the renderer would stall on exactly the request it is waiting for. The queue hands the work to
 * a background thread and answers immediately.
 *
 * This header is the pure part — a fixed-size ring with de-duplication and drop-when-full — so it
 * can be asserted without a network. The thread and the fetch policy live in prefetch.c.
 *
 * Drop-when-full is deliberate, not laziness: the queue is an optimisation. If we cannot keep up,
 * the right answer is to forget work, never to block the server or grow without bound. A dropped
 * prefetch costs one later cache miss; a blocked accept loop costs the flight view.
 */
#ifndef FB_PREFETCH_H
#define FB_PREFETCH_H

#define FB_PF_CAP 4096          /* ~one full near+far grid of imagery children, with room to spare */

typedef struct { int kind; int z; long x, y; } fb_pf_job;

typedef struct {
    fb_pf_job q[FB_PF_CAP];
    int head, tail;             /* head = next to pop, tail = next free; empty when equal */
    long pushed, dropped, deduped;
} fb_pf_queue;

static void fb_pf_init(fb_pf_queue *p){
    p->head = p->tail = 0;
    p->pushed = p->dropped = p->deduped = 0;
}

static int fb_pf_count(const fb_pf_queue *p){
    int n = p->tail - p->head;
    return n < 0 ? n + FB_PF_CAP : n;
}

/* Is this job already waiting? Keeps a hot area from queueing the same tile on every request. */
static int fb_pf_has(const fb_pf_queue *p, int kind, int z, long x, long y){
    for(int i = p->head; i != p->tail; i = (i + 1) % FB_PF_CAP)
        if(p->q[i].kind == kind && p->q[i].z == z && p->q[i].x == x && p->q[i].y == y) return 1;
    return 0;
}

/* Returns 1 if queued, 0 if dropped (full) or already present. Never blocks, never grows. */
static int fb_pf_push(fb_pf_queue *p, int kind, int z, long x, long y){
    if(fb_pf_count(p) >= FB_PF_CAP - 1){ p->dropped++; return 0; }   /* full: forget it */
    if(fb_pf_has(p, kind, z, x, y)){ p->deduped++; return 0; }
    p->q[p->tail].kind = kind; p->q[p->tail].z = z; p->q[p->tail].x = x; p->q[p->tail].y = y;
    p->tail = (p->tail + 1) % FB_PF_CAP;
    p->pushed++;
    return 1;
}

/* Returns 1 and fills *out, or 0 when empty. */
static int fb_pf_pop(fb_pf_queue *p, fb_pf_job *out){
    if(p->head == p->tail) return 0;
    *out = p->q[p->head];
    p->head = (p->head + 1) % FB_PF_CAP;
    return 1;
}

#endif /* FB_PREFETCH_H */
