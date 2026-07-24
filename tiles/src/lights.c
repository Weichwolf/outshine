#define _GNU_SOURCE
#include "lights.h"
#include "cache.h"
#include "tilesrc.h"
#include "tilemath.h"
#include <osmmesh/osmmesh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <pthread.h>

/* ---- wire format ------------------------------------------------------------------------------
 * Header {u16 count, u16 reserved=0} then count * {u16 x, u16 y (tile-local, 0..65535), u8 class,
 * u8 intensity} = 6 B/light, cap FB_LIGHTS_CAP (~24 KB/tile max). Manually little-endian packed
 * (not a memcpy'd struct): the boundary between this C server and whatever decodes the bytes
 * (JS/WASM client, still to be written) must not depend on host endianness or struct padding. */
#define FB_LIGHTS_CAP        4096
#define FB_LIGHTS_RAW_MAX   16384   /* generation-time candidate cap; a hard memory bound, not the
                                     * priority decision itself (that happens in finalize()) */
#define FB_LIGHTS_MAXZ_INDIV   12   /* z >= this: generate directly from the vector tile.
                                     * z <  this: aggregate the 4 z+1 children (recursive pyramid). */
#define FB_LIGHTS_AGG_GRID     32
#define FB_LIGHTS_AGG_CAP     (FB_LIGHTS_AGG_GRID*FB_LIGHTS_AGG_GRID)  /* 1024 */

enum { FB_LC_RESIDENTIAL=0, FB_LC_PRIMARY=1, FB_LC_MOTORWAY=2, FB_LC_BLDG_WARM=3,
       FB_LC_BLDG_COOL=4, FB_LC_AERODROME=5, FB_LC_TOWER=6, FB_LC_GLOW=7 };

typedef struct { uint16_t x, y; uint8_t cls, inten; } fb_light;

static char g_dir[300] = "/var/cache/fbtiles";
static long g_baked = 0, g_served = 0;

int fb_lights_init(const char *dir){
    if(dir && *dir) snprintf(g_dir, sizeof g_dir, "%s", dir);
    char sub[340]; snprintf(sub, sizeof sub, "%s/lights_v1", g_dir);
    mkdir(sub, 0755);
    return 0;
}

void fb_lights_stats(long *baked, long *served){
    if(baked)  *baked  = __atomic_load_n(&g_baked,  __ATOMIC_RELAXED);
    if(served) *served = __atomic_load_n(&g_served, __ATOMIC_RELAXED);
}

static void path_of(int z, long x, long y, char *p, size_t n){
    snprintf(p, n, "%s/lights_v1/%d_%ld_%ld.bin", g_dir, z, x, y);
}

static uint8_t *read_file(const char *p, size_t *n){
    FILE *f = fopen(p, "rb"); if(!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if(sz <= 0){ fclose(f); return 0; }
    uint8_t *b = malloc((size_t)sz);
    if(!b || fread(b, 1, (size_t)sz, f) != (size_t)sz){ free(b); fclose(f); return 0; }
    fclose(f); *n = (size_t)sz; return b;
}

int fb_lights_ondisk(int z, long x, long y, uint8_t **out, size_t *n){
    char p[400]; path_of(z, x, y, p, sizeof p);
    struct stat st;
    if(stat(p, &st) != 0 || st.st_size <= 0) return 0;
    *out = read_file(p, n);
    if(!*out) return 0;
    /* counts here, not in fb_lights_get: main.c's route handler calls fb_lights_ondisk directly for
     * the fast (already-cached) path -- same split bake.c uses (fb_bake_ondisk owns g_hits) -- so a
     * counter placed in fb_lights_get would miss the common case entirely and lights_served would
     * read near-zero forever. */
    __atomic_fetch_add(&g_served, 1, __ATOMIC_RELAXED);
    return 1;
}

static int write_disk(int z, long x, long y, const uint8_t *body, size_t n){
    char p[400]; path_of(z, x, y, p, sizeof p);
    char tmp[420]; snprintf(tmp, sizeof tmp, "%s.%lu.tmp", p, (unsigned long)pthread_self());
    FILE *f = fopen(tmp, "wb");
    if(!f) return 0;
    int ok = fwrite(body, 1, n, f) == n;
    fclose(f);
    if(ok) rename(tmp, p); else remove(tmp);
    return ok;
}

/* ---- deterministic hashing ----------------------------------------------------------------------
 * No rand() anywhere in this file: every inclusion/phase/intensity-jitter decision below is a pure
 * function of (z,x,y,feature/sample index), so re-baking a tile (cache eviction, style bump) always
 * reproduces the SAME light set -- lights must not visibly pop between bakes. */
static uint64_t fb_mix64(uint64_t x){
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
static uint64_t fb_lhash(int z, long x, long y, uint32_t a, uint32_t b){
    uint64_t h = fb_mix64((uint64_t)(uint32_t)z ^ 0x1469598103934665ULL);
    h = fb_mix64(h ^ (uint64_t)(uint32_t)x);
    h = fb_mix64(h ^ (uint64_t)(uint32_t)y);
    h = fb_mix64(h ^ (((uint64_t)a << 32) | b));
    return h;
}

/* ---- tiny MVT helpers (duplicated from raster.c's file-local statics -- not worth a shared header
 * for three one-line lookups) ------------------------------------------------------------------- */
static const osmmesh_mvt_layer *layer_named(osmmesh_mvt_tile *t, const char *name){
    size_t nl = osmmesh_mvt_num_layers(t);
    for(size_t i=0;i<nl;i++){
        const osmmesh_mvt_layer *l = osmmesh_mvt_layer_at(t,i);
        if(!strcmp(osmmesh_mvt_layer_name(l), name)) return l;
    }
    return 0;
}
static int tag_str(const osmmesh_mvt_layer *l, const osmmesh_mvt_feature *f, const char *key,
                   const char **out){
    for(size_t p=0;p<f->n_tag_pairs;p++){
        const char *k = osmmesh_mvt_feature_tag_key(l,f,p);
        if(k && !strcmp(k,key)){
            osmmesh_mvt_value v;
            if(osmmesh_mvt_feature_tag_value(l,f,p,&v)==0 && v.type==OSMMESH_MVT_VAL_STRING){
                *out = v.v.s; return 1;
            }
            return 0;
        }
    }
    return 0;
}

/* Even-odd point-in-polygon, same half-open-interval crossing rule as fb_draw_fill (draw.h) --
 * proven correct there, reused here for a single query point instead of a whole scanline. Point and
 * polygon must already be in the SAME coordinate space (caller rescales when layer extents differ). */
static int fb_pip(const osmmesh_mvt_feature *ft, float px, float py){
    if(ft->n_coords < 3 || !ft->coords) return 0;
    size_t nr = ft->n_rings ? ft->n_rings : 1;
    int inside = 0;
    for(size_t ring=0; ring<nr; ring++){
        size_t a  = ft->n_rings ? ft->ring_offsets[ring]   : 0;
        size_t bb = ft->n_rings ? ft->ring_offsets[ring+1] : ft->n_coords;
        for(size_t k=a;k<bb;k++){
            size_t k2 = (k+1<bb) ? k+1 : a;
            float ya=ft->coords[k].y,  yb=ft->coords[k2].y;
            float xa=ft->coords[k].x,  xb=ft->coords[k2].x;
            if((ya<=py && yb>py) || (yb<=py && ya>py)){
                float xi = xa + (py-ya)/(yb-ya)*(xb-xa);
                if(xi > px) inside = !inside;
            }
        }
    }
    return inside;
}

/* Bounding box first, full ring walk only if the point could plausibly be inside -- land polygons
 * carry thousands of vertices at low zoom (a whole country's multipolygon), and gen_buildings below
 * calls this once per (building x candidate landuse feature) pair. */
typedef struct { float x0,y0,x1,y1; } fb_bbox;
static fb_bbox fb_bbox_of(const osmmesh_mvt_feature *ft){
    fb_bbox b = { 1e9f, 1e9f, -1e9f, -1e9f };
    for(size_t i=0;i<ft->n_coords;i++){
        float x=ft->coords[i].x, y=ft->coords[i].y;
        if(x<b.x0) b.x0=x;
        if(x>b.x1) b.x1=x;
        if(y<b.y0) b.y0=y;
        if(y>b.y1) b.y1=y;
    }
    return b;
}
static int fb_bbox_hit(const fb_bbox *b, float px, float py){
    return px>=b->x0 && px<=b->x1 && py>=b->y0 && py<=b->y1;
}

/* True area centroid (shoelace), ring 0 (outer) only -- falls back to a plain vertex average for a
 * degenerate near-zero-area ring (duplicated points, a hairline sliver) rather than dividing by ~0. */
static void fb_centroid(const osmmesh_mvt_feature *ft, float *cx, float *cy){
    size_t a  = ft->n_rings ? ft->ring_offsets[0] : 0;
    size_t bb = ft->n_rings ? ft->ring_offsets[1] : ft->n_coords;
    double A=0, Cx=0, Cy=0;
    for(size_t k=a;k<bb;k++){
        size_t k2 = (k+1<bb) ? k+1 : a;
        double xa=ft->coords[k].x, ya=ft->coords[k].y, xb=ft->coords[k2].x, yb=ft->coords[k2].y;
        double cross = xa*yb - xb*ya;
        A += cross; Cx += (xa+xb)*cross; Cy += (ya+yb)*cross;
    }
    A *= 0.5;
    if(fabs(A) > 1e-6){ *cx=(float)(Cx/(6.0*A)); *cy=(float)(Cy/(6.0*A)); return; }
    double sx=0, sy=0; size_t n = bb>a ? bb-a : 0;
    for(size_t k=a;k<bb;k++){ sx+=ft->coords[k].x; sy+=ft->coords[k].y; }
    *cx = n ? (float)(sx/(double)n) : 0.0f;
    *cy = n ? (float)(sy/(double)n) : 0.0f;
}

/* Hand-tuned palette (cosmetic, like w3_landcolor/w3_roadstyle in style.h): base + hash-jittered
 * spread per class, not a physical brightness model. */
static uint8_t base_intensity(int cls, uint64_t h){
    static const uint8_t base[8]   = {140,170,200, 90,130,220,255,  0};
    static const uint8_t jitter[8] = { 40, 40, 30, 60, 50, 20,  0,  0};
    int v = base[cls] + (jitter[cls] ? (int)(h % (jitter[cls]+1u)) : 0);
    return (uint8_t)(v > 255 ? 255 : v);
}

static void emit(fb_light *raw, int *nraw, uint16_t x, uint16_t y, uint8_t cls, uint8_t inten){
    if(*nraw < FB_LIGHTS_RAW_MAX) raw[(*nraw)++] = (fb_light){x,y,cls,inten};
}

/* Keep-first-N priority when over cap: rare, safety/orientation-relevant classes survive a hard cap
 * before the numerous, purely-decorative building windows do. */
static int class_priority(int cls){
    static const int pr[8] = { 60, 70, 80, 40, 50, 90, 100, 20 };
    return (cls>=0 && cls<8) ? pr[cls] : 0;
}
static int cmp_priority(const void *pa, const void *pb){
    const fb_light *a=pa, *b=pb;
    int pa_=class_priority(a->cls), pb_=class_priority(b->cls);
    if(pa_ != pb_) return pb_ - pa_;
    /* deterministic tiebreak: qsort's ordering among equal keys is unspecified, and an unstable tie
     * would let a capped tile's surviving subset change across re-bakes with identical input. */
    uint32_t ka = ((uint32_t)a->x<<16)|a->y, kb = ((uint32_t)b->x<<16)|b->y;
    return (ka>kb) - (ka<kb);
}

static uint8_t *serialize(fb_light *raw, int nraw, int cap, size_t *outlen){
    int n = nraw;
    if(n > cap){ qsort(raw, (size_t)n, sizeof(fb_light), cmp_priority); n = cap; }
    uint8_t *buf = malloc(4 + (size_t)n*6);
    buf[0]=(uint8_t)(n&0xFF); buf[1]=(uint8_t)((n>>8)&0xFF); buf[2]=0; buf[3]=0;
    for(int i=0;i<n;i++){
        uint8_t *p = buf+4+i*6;
        p[0]=(uint8_t)(raw[i].x&0xFF);   p[1]=(uint8_t)((raw[i].x>>8)&0xFF);
        p[2]=(uint8_t)(raw[i].y&0xFF);   p[3]=(uint8_t)((raw[i].y>>8)&0xFF);
        p[4]=raw[i].cls; p[5]=raw[i].inten;
    }
    *outlen = 4 + (size_t)n*6;
    return buf;
}

/* ---- street-light sampling ----------------------------------------------------------------------
 * Walks the polyline's cumulative arc length and drops a light every `spacing_m` metres (converted
 * to this layer's tile-local units via fb_tile_res_m), with a per-feature deterministic phase offset
 * so every road doesn't visibly start its first light exactly at vertex 0. */
static void sample_line(const osmmesh_mvt_feature *ft, float meters_per_unit, float spacing_m,
                        int cls, int require_builtup, float to_land_extent,
                        const fb_bbox *builtup_bbox, const osmmesh_mvt_feature **builtup, int nbuiltup,
                        int z, long x, long y, uint32_t featidx, int extent,
                        fb_light *raw, int *nraw){
    if(ft->n_coords < 2 || !ft->coords) return;
    float spacing_units = spacing_m / meters_per_unit;
    if(spacing_units < 1.0f) spacing_units = 1.0f;
    size_t nr = ft->n_rings ? ft->n_rings : 1;
    uint32_t sampleidx = 0;

    /* A LINESTRING feature can be a MultiLineString of several DISJOINT parts (ring_offsets splits
     * them, same convention fb_draw_line/draw.h already uses for lines and fb_draw_fill for polygon
     * rings) -- walking ring_offsets[0]..ring_offsets[n_rings] as one span would splice a phantom
     * connecting segment between the end of one part and the start of the next, often kilometres
     * long, which then gets dutifully sampled every spacing_units and floods the tile with bogus
     * lights. Each ring is its own polyline; the arc-length walk resets at every ring. */
    for(size_t ring=0; ring<nr; ring++){
        size_t a  = ft->n_rings ? ft->ring_offsets[ring]   : 0;
        size_t bb = ft->n_rings ? ft->ring_offsets[ring+1] : ft->n_coords;
        if(bb < a+2) continue;

        double phase = (double)(fb_lhash(z,x,y,featidx,0xA100+(uint32_t)ring) % 1000000ull)
                       / 1000000.0 * spacing_units;
        double next = phase, consumed = 0.0;
        for(size_t k=a+1; k<bb; k++){
            float xa=ft->coords[k-1].x, ya=ft->coords[k-1].y;
            float xb=ft->coords[k].x,   yb=ft->coords[k].y;
            double dx=xb-xa, dy=yb-ya, seglen=sqrt(dx*dx+dy*dy);
            if(seglen < 1e-6) continue;
            while(next <= consumed + seglen){
                double t = (next - consumed) / seglen;
                float px = (float)(xa + dx*t), py = (float)(ya + dy*t);
                next += spacing_units;
                sampleidx++;
                if(px < 0 || px >= (float)extent || py < 0 || py >= (float)extent) continue;
                if(require_builtup){
                    /* land polygons live in the LAND layer's own MVT extent, which need not match
                     * the streets layer's -- rescale the query point before testing, same as
                     * gen_buildings does for building centroids. */
                    float lx = px*to_land_extent, ly = py*to_land_extent;
                    int ok = 0;
                    for(int i=0;i<nbuiltup;i++)
                        if(fb_bbox_hit(&builtup_bbox[i], lx, ly) && fb_pip(builtup[i], lx, ly)){ ok=1; break; }
                    if(!ok) continue;
                }
                uint16_t qx=(uint16_t)(px/(float)extent*65536.0f), qy=(uint16_t)(py/(float)extent*65536.0f);
                emit(raw, nraw, qx, qy, (uint8_t)cls, base_intensity(cls, fb_lhash(z,x,y,featidx,sampleidx)));
            }
            consumed += seglen;
        }
    }
}

static void gen_buildings(const osmmesh_mvt_layer *bL, const osmmesh_mvt_layer *landL,
                          int z, long x, long y, fb_light *raw, int *nraw){
    if(!bL) return;
    int ext = (int)osmmesh_mvt_layer_extent(bL);
    int lext = landL ? (int)osmmesh_mvt_layer_extent(landL) : ext;
    float toLand = landL ? (float)lext/(float)ext : 1.0f;

    /* Precompute bbox once per land feature carrying a residential/commercial/industrial kind
     * (the only kinds this classification cares about), reused for every building below. */
    size_t nl = landL ? osmmesh_mvt_num_features(landL) : 0;
    const osmmesh_mvt_feature **lu = nl ? malloc(sizeof(*lu)*nl) : 0;
    fb_bbox *lubb = nl ? malloc(sizeof(*lubb)*nl) : 0;
    const char **lukind = nl ? malloc(sizeof(*lukind)*nl) : 0;
    int nlu = 0;
    for(size_t li=0; li<nl; li++){
        const osmmesh_mvt_feature *lf = osmmesh_mvt_feature_at(landL, li);
        if(lf->geom_type != OSMMESH_MVT_GEOM_POLYGON) continue;
        const char *k;
        if(!tag_str(landL, lf, "kind", &k)) continue;
        if(strcmp(k,"residential") && strcmp(k,"commercial") && strcmp(k,"industrial")) continue;
        lu[nlu]=lf; lubb[nlu]=fb_bbox_of(lf); lukind[nlu]=k; nlu++;
    }

    size_t nf = osmmesh_mvt_num_features(bL);
    for(size_t f=0; f<nf; f++){
        const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(bL, f);
        if(ft->geom_type != OSMMESH_MVT_GEOM_POLYGON) continue;
        float cx, cy; fb_centroid(ft, &cx, &cy);
        if(cx<0 || cx>=(float)ext || cy<0 || cy>=(float)ext) continue;

        const char *luKind = 0;
        float lx = cx*toLand, ly = cy*toLand;
        for(int i=0; i<nlu && !luKind; i++)
            if(fb_bbox_hit(&lubb[i], lx, ly) && fb_pip(lu[i], lx, ly)) luKind = lukind[i];

        int cls, N;
        if(luKind && (!strcmp(luKind,"commercial") || !strcmp(luKind,"industrial"))){ cls=FB_LC_BLDG_COOL; N=5; }
        else if(luKind){ cls=FB_LC_BLDG_WARM; N=8; }
        else { cls=FB_LC_BLDG_WARM; N=20; }

        uint64_t h = fb_lhash(z,x,y,(uint32_t)f,0xB01D);
        if(h % (uint32_t)N != 0) continue;
        uint16_t qx=(uint16_t)(cx/(float)ext*65536.0f), qy=(uint16_t)(cy/(float)ext*65536.0f);
        emit(raw, nraw, qx, qy, (uint8_t)cls, base_intensity(cls, h));
    }
    free(lu); free(lubb); free(lukind);
}

static int generate_individual(int z, long x, long y, uint8_t **out, size_t *n){
    uint8_t *vec=0; size_t vn=0;
    if(!fb_cache_get(FB_TILE_VECTOR, z, x, y, &vec, &vn)) return 0;
    osmmesh_mvt_tile *t=0;
    int rc = osmmesh_mvt_decode(vec, vn, &t);
    free(vec);
    if(rc != OSMMESH_MVT_OK) return 0;

    fb_light *raw = malloc(sizeof(fb_light)*FB_LIGHTS_RAW_MAX);
    int nraw = 0;
    double lat, lon; fb_tile_to_geo(x+0.5, y+0.5, z, &lat, &lon); (void)lon;

    const osmmesh_mvt_layer *streetsL = layer_named(t, "streets");
    const osmmesh_mvt_layer *landL    = layer_named(t, "land");
    const osmmesh_mvt_layer *bldgL    = layer_named(t, "buildings");
    const osmmesh_mvt_layer *ptL      = layer_named(t, "public_transport");
    const osmmesh_mvt_layer *poisL    = layer_named(t, "pois");

    if(streetsL){
        int ext = (int)osmmesh_mvt_layer_extent(streetsL);
        float mpu = (float)(fb_tile_res_m(lat,z) * 256.0 / ext);
        int lext = landL ? (int)osmmesh_mvt_layer_extent(landL) : ext;
        float toLand = landL ? (float)lext/(float)ext : 1.0f;

        /* built-up landuse (residential/commercial/industrial), precomputed once: gates class-0
         * (residential/tertiary) street sampling per spec -- "only inside built-up land, motorways
         * always". Primary/secondary is intentionally left ungated (spec names only these two cases
         * explicitly; a rural primary road is common and still plausibly lit at intersections/villages,
         * unlike a rural "residential"-tagged track). */
        size_t nl = landL ? osmmesh_mvt_num_features(landL) : 0;
        const osmmesh_mvt_feature **builtup = nl ? malloc(sizeof(*builtup)*nl) : 0;
        fb_bbox *builtup_bb = nl ? malloc(sizeof(*builtup_bb)*nl) : 0;
        int nbuiltup = 0;
        for(size_t li=0; li<nl; li++){
            const osmmesh_mvt_feature *lf = osmmesh_mvt_feature_at(landL, li);
            if(lf->geom_type != OSMMESH_MVT_GEOM_POLYGON) continue;
            const char *k;
            if(!tag_str(landL, lf, "kind", &k)) continue;
            if(strcmp(k,"residential") && strcmp(k,"commercial") && strcmp(k,"industrial")) continue;
            builtup[nbuiltup] = lf; builtup_bb[nbuiltup] = fb_bbox_of(lf); nbuiltup++;
        }

        size_t nf = osmmesh_mvt_num_features(streetsL);
        for(size_t f=0; f<nf; f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(streetsL, f);
            if(ft->geom_type != OSMMESH_MVT_GEOM_LINESTRING) continue;
            const char *k;
            if(!tag_str(streetsL, ft, "kind", &k)) continue;
            int cls, gate = 0; float spacing;
            if(!strcmp(k,"residential") || !strcmp(k,"tertiary")){ cls=FB_LC_RESIDENTIAL; spacing=50; gate=1; }
            else if(!strcmp(k,"primary") || !strcmp(k,"secondary")){ cls=FB_LC_PRIMARY; spacing=35; }
            else if(!strcmp(k,"motorway") || !strcmp(k,"trunk")){ cls=FB_LC_MOTORWAY; spacing=60; }
            else if(!strcmp(k,"runway")){ cls=FB_LC_AERODROME; spacing=60; }
            else continue;
            sample_line(ft, mpu, spacing, cls, gate, toLand, builtup_bb, builtup, nbuiltup,
                       z, x, y, (uint32_t)f, ext, raw, &nraw);
        }
        free(builtup); free(builtup_bb);
    }

    if(bldgL) gen_buildings(bldgL, landL, z, x, y, raw, &nraw);

    if(ptL){
        int ext = (int)osmmesh_mvt_layer_extent(ptL);
        size_t nf = osmmesh_mvt_num_features(ptL);
        for(size_t f=0; f<nf; f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(ptL, f);
            if(ft->geom_type != OSMMESH_MVT_GEOM_POINT || ft->n_coords < 1) continue;
            const char *k;
            if(!tag_str(ptL, ft, "kind", &k) || strcmp(k,"aerodrome")) continue;
            float px = ft->coords[0].x, py = ft->coords[0].y;
            if(px<0||px>=(float)ext||py<0||py>=(float)ext) continue;
            uint16_t qx=(uint16_t)(px/(float)ext*65536.0f), qy=(uint16_t)(py/(float)ext*65536.0f);
            emit(raw, &nraw, qx, qy, FB_LC_AERODROME, base_intensity(FB_LC_AERODROME, fb_lhash(z,x,y,(uint32_t)f,0xBEAC)));
        }
    }

    if(poisL){
        int ext = (int)osmmesh_mvt_layer_extent(poisL);
        size_t nf = osmmesh_mvt_num_features(poisL);
        for(size_t f=0; f<nf; f++){
            const osmmesh_mvt_feature *ft = osmmesh_mvt_feature_at(poisL, f);
            if(ft->geom_type != OSMMESH_MVT_GEOM_POINT || ft->n_coords < 1) continue;
            const char *mm;
            if(!tag_str(poisL, ft, "man_made", &mm)) continue;
            if(strcmp(mm,"tower") && strcmp(mm,"mast") && strcmp(mm,"communications_tower") && strcmp(mm,"chimney")) continue;
            float px = ft->coords[0].x, py = ft->coords[0].y;
            if(px<0||px>=(float)ext||py<0||py>=(float)ext) continue;
            uint16_t qx=(uint16_t)(px/(float)ext*65536.0f), qy=(uint16_t)(py/(float)ext*65536.0f);
            emit(raw, &nraw, qx, qy, FB_LC_TOWER, base_intensity(FB_LC_TOWER, fb_lhash(z,x,y,(uint32_t)f,0x70E1)));
        }
    }

    osmmesh_mvt_free(t);
    *out = serialize(raw, nraw, FB_LIGHTS_CAP, n);
    free(raw);
    return 1;
}

/* z < FB_LIGHTS_MAXZ_INDIV: recurse into the 4 z+1 children (each independently disk-cached, so a
 * repeat request anywhere in the pyramid is cheap), bin their lights into a 32x32 grid over this
 * tile's own local space, and emit one class-7 "glow" point per non-empty cell -- exactly the cap
 * (1024) needed, no separate priority drop required at this level. */
static int aggregate(int z, long x, long y, uint8_t **out, size_t *n){
    double sumI[FB_LIGHTS_AGG_CAP] = {0}, sumX[FB_LIGHTS_AGG_CAP] = {0}, sumY[FB_LIGHTS_AGG_CAP] = {0};
    int used[FB_LIGHTS_AGG_CAP]; memset(used, 0, sizeof used);
    int any = 0;

    for(int dy=0; dy<2; dy++) for(int dx=0; dx<2; dx++){
        long cx = x*2+dx, cy = y*2+dy;
        uint8_t *cbuf=0; size_t cn=0;
        if(!fb_lights_get(z+1, cx, cy, &cbuf, &cn)) continue;
        any = 1;
        if(cn >= 4){
            int cnt = cbuf[0] | (cbuf[1]<<8);
            for(int i=0; i<cnt && 4+(size_t)(i+1)*6 <= cn; i++){
                const uint8_t *p = cbuf + 4 + i*6;
                uint32_t lx = p[0] | (p[1]<<8), ly = p[2] | (p[3]<<8);
                uint8_t inten = p[5];
                uint32_t px = (uint32_t)dx*32768u + (lx>>1);
                uint32_t py = (uint32_t)dy*32768u + (ly>>1);
                int gx = (int)(px * (uint32_t)FB_LIGHTS_AGG_GRID / 65536u);
                int gy = (int)(py * (uint32_t)FB_LIGHTS_AGG_GRID / 65536u);
                if(gx>=FB_LIGHTS_AGG_GRID) gx=FB_LIGHTS_AGG_GRID-1;
                if(gy>=FB_LIGHTS_AGG_GRID) gy=FB_LIGHTS_AGG_GRID-1;
                int cell = gy*FB_LIGHTS_AGG_GRID+gx;
                sumI[cell] += inten; sumX[cell] += (double)px*inten; sumY[cell] += (double)py*inten;
                used[cell] = 1;
            }
        }
        free(cbuf);
    }
    if(!any) return 0;

    fb_light *raw = malloc(sizeof(fb_light)*FB_LIGHTS_AGG_CAP);
    int nraw = 0;
    for(int c=0; c<FB_LIGHTS_AGG_CAP; c++){
        if(!used[c]) continue;
        double cx = sumX[c]/sumI[c], cy = sumY[c]/sumI[c];
        double v = log2(1.0 + sumI[c]) * 22.0;  /* log-summed so a 100-light metro cell doesn't just
                                                  * clip to 255 the same as a 3-light village cell */
        uint8_t inten = (uint8_t)(v > 255.0 ? 255 : (v < 0 ? 0 : v));
        raw[nraw++] = (fb_light){ (uint16_t)cx, (uint16_t)cy, FB_LC_GLOW, inten };
    }
    *out = serialize(raw, nraw, FB_LIGHTS_AGG_CAP, n);
    free(raw);
    return 1;
}

/* Same inflight-dedup pattern as bakepool.c, keyed by (z,x,y) -- lights tiles have no kind/TS axis.
 * Wrapped INSIDE fb_lights_get() itself, not at the /t/lights route: aggregate() recursively calls
 * fb_lights_get() for its 4 children (the z<12 pyramid), so putting dedup here covers that internal
 * recursion for free, not just the top-level route. Unlike bakepool.c's single reply-then-free per
 * consumer, fb_lights_get's documented contract is "every caller gets its OWN malloc'd buffer to
 * free" (recursive aggregate() callers and the route handler alike) -- so each waiter takes an
 * independent copy of the shared result under the lock, and the shared buffer itself is freed once,
 * by whichever consumer happens to be last. */
#define FB_LT_SLOTS 512
typedef struct {
    int used, done, ok, refcount;
    int z; long x, y;
    uint8_t *body; size_t n;
} fb_lights_slot;

static fb_lights_slot  g_lslot[FB_LT_SLOTS];
static pthread_mutex_t g_lmx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_lcv = PTHREAD_COND_INITIALIZER;

static fb_lights_slot *lslot_find(int z, long x, long y){
    for(int i=0;i<FB_LT_SLOTS;i++){
        fb_lights_slot *s=&g_lslot[i];
        if(s->used && s->z==z && s->x==x && s->y==y) return s;
    }
    return 0;
}
static fb_lights_slot *lslot_alloc(int z, long x, long y){
    for(int i=0;i<FB_LT_SLOTS;i++) if(!g_lslot[i].used){
        fb_lights_slot *s=&g_lslot[i];
        memset(s, 0, sizeof *s);
        s->used=1; s->z=z; s->x=x; s->y=y;
        return s;
    }
    return 0;
}

static int lights_generate(int z, long x, long y, uint8_t **out, size_t *n){
    int ok = (z >= FB_LIGHTS_MAXZ_INDIV) ? generate_individual(z, x, y, out, n)
                                          : aggregate(z, x, y, out, n);
    if(!ok) return 0;
    write_disk(z, x, y, *out, *n);
    __atomic_fetch_add(&g_baked, 1, __ATOMIC_RELAXED);
    return 1;
}

int fb_lights_get(int z, long x, long y, uint8_t **out, size_t *n){
    if(!fb_tile_wrap(z, &x, &y)) return 0;

    int maxz = fb_src_max_zoom(FB_TILE_VECTOR);
    if(z > maxz){
        /* Beyond native vector resolution (14): no consumer needs this yet (the client side of this
         * feature is a later task) -- reuse the covering z=maxz ancestor's own light set rather than
         * inventing geometry finer than the source data has. Not cached again under the deep z/x/y:
         * that would duplicate identical bytes under an unbounded number of keys for nothing. */
        long f = 1L << (z - maxz);
        return fb_lights_get(maxz, x/f, y/f, out, n);
    }

    if(fb_lights_ondisk(z, x, y, out, n)) return 1;   /* fb_lights_ondisk itself counts the hit */

    pthread_mutex_lock(&g_lmx);
    fb_lights_slot *s = lslot_find(z, x, y);
    int owner = 0;
    if(!s){ s = lslot_alloc(z, x, y); owner = (s != 0); }
    if(!s){
        /* table exhausted (512 distinct in-flight lights tiles at once): a genuine capacity
         * emergency, not the normal cold path -- generate ungated rather than block forever. */
        pthread_mutex_unlock(&g_lmx);
        return lights_generate(z, x, y, out, n);
    }
    s->refcount++;
    pthread_mutex_unlock(&g_lmx);

    if(owner){
        uint8_t *body=0; size_t bn=0;
        int ok = lights_generate(z, x, y, &body, &bn);
        pthread_mutex_lock(&g_lmx);
        s->ok=ok; s->body=body; s->n=bn; s->done=1;
        pthread_cond_broadcast(&g_lcv);
        pthread_mutex_unlock(&g_lmx);
    } else {
        pthread_mutex_lock(&g_lmx);
        while(!s->done) pthread_cond_wait(&g_lcv, &g_lmx);
        pthread_mutex_unlock(&g_lmx);
    }

    pthread_mutex_lock(&g_lmx);
    int ok = s->ok; size_t bn = s->n;
    uint8_t *mine = 0;
    if(ok){ mine = malloc(bn); if(mine) memcpy(mine, s->body, bn); }
    s->refcount--;
    int last = (s->refcount == 0);
    if(last){ free(s->body); s->used = 0; }
    pthread_mutex_unlock(&g_lmx);

    if(!ok || !mine) return 0;
    *out = mine; *n = bn;
    return 1;
}
