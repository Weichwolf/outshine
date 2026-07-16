#define _GNU_SOURCE
#include "bake.h"
#include "raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../geo/osmmesh/src/3rdparty/stb_image_write.h"

static char g_dir[256] = "/var/cache/fbtiles";
static long g_hits = 0, g_bakes = 0, g_fail = 0;

int fb_bake_init(const char *dir){
    if(dir && *dir) snprintf(g_dir, sizeof g_dir, "%s", dir);
    mkdir(g_dir, 0755);
    char sub[320];
    snprintf(sub, sizeof sub, "%s/bake_osm",   g_dir); mkdir(sub, 0755);
    snprintf(sub, sizeof sub, "%s/bake_photo", g_dir); mkdir(sub, 0755);
    return 0;
}

static void bake_path(fb_albedo_kind k, int z, long x, long y, int TS, char *p, size_t n){
    snprintf(p, n, "%s/bake_%s/%d_%d_%ld_%ld.%s", g_dir,
             k==FB_ALBEDO_PHOTO ? "photo" : "osm", TS, z, x, y,
             k==FB_ALBEDO_PHOTO ? "jpg" : "png");
}

static uint8_t *read_file(const char *p, size_t *n){
    FILE *f = fopen(p, "rb"); if(!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if(sz <= 0){ fclose(f); return 0; }
    uint8_t *b = malloc((size_t)sz);
    if(!b || fread(b, 1, (size_t)sz, f) != (size_t)sz){ free(b); fclose(f); return 0; }
    fclose(f); *n = (size_t)sz; return b;
}

typedef struct { uint8_t *b; size_t n, cap; } membuf;
static void mem_write(void *ctx, void *data, int size){
    membuf *m = (membuf*)ctx;
    if(m->n + (size_t)size > m->cap){
        size_t cap = (m->cap ? m->cap*2 : 1<<16);
        while(cap < m->n + (size_t)size) cap *= 2;
        uint8_t *t = realloc(m->b, cap); if(!t) return;
        m->b = t; m->cap = cap;
    }
    memcpy(m->b + m->n, data, (size_t)size); m->n += (size_t)size;
}

int fb_bake_ondisk(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n){
    if(TS < 64 || TS > 4096 || (TS & (TS-1))) return 0;
    char path[400]; bake_path(k, z, x, y, TS, path, sizeof path);
    struct stat st;
    if(stat(path, &st) != 0 || st.st_size <= 0) return 0;
    *out = read_file(path, n);
    if(!*out) return 0;
    g_hits++;
    return 1;
}

int fb_bake_get(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n){
    if(TS < 64 || TS > 4096 || (TS & (TS-1))) return 0;
    if(fb_bake_ondisk(k, z, x, y, TS, out, n)) return 1;
    char path[400]; bake_path(k, z, x, y, TS, path, sizeof path);

    uint8_t *rgb = malloc((size_t)TS*TS*3);
    if(!rgb){ g_fail++; return 0; }
    if(!fb_raster_bake(k, z, x, y, TS, rgb)){ free(rgb); g_fail++; return 0; }

    membuf m = {0};
    if(k == FB_ALBEDO_PHOTO) stbi_write_jpg_to_func(mem_write, &m, TS, TS, 3, rgb, 88);
    else                     stbi_write_png_to_func(mem_write, &m, TS, TS, 3, rgb, TS*3);
    free(rgb);
    if(!m.n){ free(m.b); g_fail++; return 0; }

    char tmp[420]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if(f){
        int ok = fwrite(m.b, 1, m.n, f) == m.n;
        fclose(f);
        if(ok) rename(tmp, path); else remove(tmp);
    }
    *out = m.b; *n = m.n; g_bakes++;
    return 1;
}

void fb_bake_stats(long *hits, long *bakes, long *fails){
    if(hits) *hits = g_hits;
    if(bakes) *bakes = g_bakes;
    if(fails) *fails = g_fail;
}
