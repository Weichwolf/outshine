/* The tile-streaming C ABI; full contract table: doc/flightbox/world-and-terrain.md, Abschnitt 3. */
#ifndef FBTERRAINLOADER_H
#define FBTERRAINLOADER_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FB_TERRAIN_MAX_TILES 64

typedef struct {
  float   *verts;    /* merged w3_vtx: pos3+uv2+norm3 (8 floats), pos = ECEF offset from tile origin */
  uint32_t nverts;   /* total vertices across all tiles */
  int      ntiles;
  uint32_t voff[FB_TERRAIN_MAX_TILES];      /* first vertex of tile i in verts[] */
  uint32_t vcnt[FB_TERRAIN_MAX_TILES];      /* vertex count of tile i */
  double   origin[FB_TERRAIN_MAX_TILES][3]; /* per-tile ECEF origin (double) */
  double   center[3];                        /* field-centre ECEF (double), the camera look-at anchor */
  uint8_t *albedo;   /* ntiles layers of ts*ts RGBA8 (sRGB), layer i = tile i; real bake or fallback */
  int      albedo_ts;                        /* albedo layer edge length in texels (512) */
} fb_terrain;

/* The static one-shot bring-up path: a 4x4 field, blocking, merged into ONE vertex array. */
int  fb_terrain_load(const char *base, double lat, double lon, int z, int grid, fb_terrain *out);
void fb_terrain_free(fb_terrain *t);

/* Open once, then poll each frame: a poll kicks the fetch on a miss and returns 0 until the bytes
 * are resident. Nothing here ever blocks the browser's render loop. */
int  fb_stream_open(const char *base, double lat, double lon, int z);
void fb_stream_set_base(int mode);          /* boot base mode (0 osm, 1 photo) — worker priority hint */
void fb_stream_campos(double lat, double lon);   /* live camera track for the worker's nearest-first pump */
int  fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                     double origin[3], float *err);   /* err = geometric error (m), drives the SSE LOD */
/* Levels 0..N packed contiguous; mode 0 = /bake/osm, 1 = /bake/photo. >0 resident, 0 pending,
 * -1 a real 204 hole. */
int  fb_stream_pyramid(int z, uint32_t x, uint32_t y, int mode, int ts, uint8_t *dst);
void fb_stream_close(void);

/* The SAME DEM the mesh and HUD use, so JSBSim's ground-reaction floor matches the rendered terrain.
 * <= -1e8 until a real sample has landed. */
double fb_stream_ground(double lat, double lon);

/* Raw Terrarium bytes for the height oracle, NOT the render mesh. The payload is owned by the byte
 * cache — do NOT free. 1 ready, 0 PENDING (retry; never treat as a hole), -1 a real hole. */
int fb_stream_dem(int z, int x, int y, const uint8_t **bytes, int *len);

/* Wire format: {u16 count, u16 res=0} then count*{u16 x, u16 y tile-local, u8 class, u8 intensity}.
 * >=4 bytes (the header even at count=0), 0 pending, -1 unavailable. Lowest-priority stream. */
int fb_stream_lights(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap);

/* WASM: an embedded MEMFS path; native: a disk path. RGBA8 out is malloc'd — the caller frees. */
int  fb_load_image_file(const char *path, uint8_t **rgba, int *w, int *h);
/* Blocking startup fetch of the concatenated HYG star bands. */
int  fb_fetch_stars(const char *base, uint8_t *dst, int cap);
/* Blocking text GET, url absolute or origin-relative (the WASM boot mission comes off fb-sim's OWN
 * web/ mount, not fb-tiles). dst is NUL-terminated within cap. */
int  fb_fetch_text(const char *url, char *dst, int cap);

#ifdef __cplusplus
}
#endif
#endif
