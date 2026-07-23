#ifndef FB_PREFETCH_H
#define FB_PREFETCH_H

#define FB_PF_CAP 4096

#define FB_PF_BAKE_OSM   10
#define FB_PF_BAKE_PHOTO 11
#define FB_PF_LIGHTS     12

typedef struct { int kind; int z; long x, y; int tex; } fb_pf_job;

typedef struct {
    fb_pf_job q[FB_PF_CAP];
    int head, tail;
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

static int fb_pf_has(const fb_pf_queue *p, int kind, int z, long x, long y, int tex){
    for(int i = p->head; i != p->tail; i = (i + 1) % FB_PF_CAP)
        if(p->q[i].kind == kind && p->q[i].z == z && p->q[i].x == x && p->q[i].y == y
           && p->q[i].tex == tex) return 1;
    return 0;
}

static int fb_pf_push(fb_pf_queue *p, int kind, int z, long x, long y, int tex){
    if(fb_pf_count(p) >= FB_PF_CAP - 1){ p->dropped++; return 0; }
    if(fb_pf_has(p, kind, z, x, y, tex)){ p->deduped++; return 0; }
    p->q[p->tail].kind = kind; p->q[p->tail].z = z; p->q[p->tail].x = x; p->q[p->tail].y = y;
    p->q[p->tail].tex = tex;
    p->tail = (p->tail + 1) % FB_PF_CAP;
    p->pushed++;
    return 1;
}

static int fb_pf_pop(fb_pf_queue *p, fb_pf_job *out){
    if(p->head == p->tail) return 0;
    *out = p->q[p->head];
    p->head = (p->head + 1) % FB_PF_CAP;
    return 1;
}

#endif
