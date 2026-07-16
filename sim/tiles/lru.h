#ifndef FB_LRU_H
#define FB_LRU_H

typedef struct {
    int   z; long x, y;
    unsigned touch;
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

static int fb_lru_find(fb_lru *l, int z, long x, long y){
    for(int i = 0; i < l->n; i++)
        if(l->s[i].valid && l->s[i].z == z && l->s[i].x == x && l->s[i].y == y){
            l->s[i].touch = ++l->clock;
            return i;
        }
    return -1;
}

static int fb_lru_victim(const fb_lru *l){
    int slot = 0; unsigned oldest = ~0u;
    for(int i = 0; i < l->n; i++){
        if(!l->s[i].valid) return i;
        if(l->s[i].touch < oldest){ oldest = l->s[i].touch; slot = i; }
    }
    return slot;
}

static void fb_lru_claim(fb_lru *l, int slot, int z, long x, long y){
    l->s[slot].z = z; l->s[slot].x = x; l->s[slot].y = y;
    l->s[slot].valid = 1; l->s[slot].touch = ++l->clock;
}

static int fb_lru_occupied(const fb_lru *l, int slot){ return l->s[slot].valid; }

#endif
