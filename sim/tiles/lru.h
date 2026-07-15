/* FlightBox tiles — a pure least-recently-used slot table.
 *
 * Split out of elev.c, where the eviction logic sat wedged between a network fetch and a PNG
 * decode and could therefore only be exercised by having a real DEM server answer. The logic
 * itself touches nothing external: it decides which of N slots to reuse. Now it is assertable.
 *
 * Worth testing rather than eyeballing, because an LRU never fails loudly. Evict the wrong slot
 * and everything still works — you just re-fetch tiles you already had, and the only symptom is
 * a service that is mysteriously slower than it should be. That is a bug you can stare at for
 * a long time.
 *
 * Holds no payload: the caller keeps its own parallel array (grids, buffers, whatever) indexed
 * by the same slot number. So this file knows nothing about DEMs, PNGs or HTTP.
 */
#ifndef FB_LRU_H
#define FB_LRU_H

typedef struct {
    int   z; long x, y;
    unsigned touch;       /* clock value at last use; higher = more recent */
    int   valid;
} fb_lru_slot;

typedef struct {
    fb_lru_slot *s;
    int n;
    unsigned clock;
} fb_lru;

static void fb_lru_init(fb_lru *l, fb_lru_slot *slots, int n){
    l->s = slots; l->n = n; l->clock = 0;
    for(int i = 0; i < n; i++){ slots[i].valid = 0; slots[i].touch = 0; }
}

/* Slot index holding (z,x,y), or -1. A hit marks the slot most-recently-used. */
static int fb_lru_find(fb_lru *l, int z, long x, long y){
    for(int i = 0; i < l->n; i++)
        if(l->s[i].valid && l->s[i].z == z && l->s[i].x == x && l->s[i].y == y){
            l->s[i].touch = ++l->clock;
            return i;
        }
    return -1;
}

/* Which slot to use for a new entry: a free one, else the least-recently-used.
 * Does NOT claim it — the caller must free whatever payload lives there first, then claim. */
static int fb_lru_victim(const fb_lru *l){
    int slot = 0; unsigned oldest = ~0u;
    for(int i = 0; i < l->n; i++){
        if(!l->s[i].valid) return i;                    /* a free slot always wins */
        if(l->s[i].touch < oldest){ oldest = l->s[i].touch; slot = i; }
    }
    return slot;
}

/* Take ownership of a slot for (z,x,y) and mark it most-recently-used. */
static void fb_lru_claim(fb_lru *l, int slot, int z, long x, long y){
    l->s[slot].z = z; l->s[slot].x = x; l->s[slot].y = y;
    l->s[slot].valid = 1; l->s[slot].touch = ++l->clock;
}

/* Was this slot holding something? (the caller has to free its payload before claiming) */
static int fb_lru_occupied(const fb_lru *l, int slot){ return l->s[slot].valid; }

#endif /* FB_LRU_H */
