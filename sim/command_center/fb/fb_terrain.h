/* FlightBox WebGPU demo — real-terrain loader (see fb_terrain.c). One blocking call fills a merged
 * camera-relative ECEF mesh (w3_vtx) plus per-tile ECEF origins the renderer subtracts each frame. */
#ifndef FB_TERRAIN_H
#define FB_TERRAIN_H
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

/* Fetch+mesh a 4x4 field of DEM tiles at zoom z around (lat,lon) from fb-tiles `base`
 * (e.g. "http://localhost:8081"). `grid` = terrain decimation (e.g. 32). Returns 1 on success. */
int  fb_terrain_load(const char *base, double lat, double lon, int z, int grid, fb_terrain *out);
void fb_terrain_free(fb_terrain *t);

/* ---- streaming (Stage 4): NON-BLOCKING per-tile fetch/mesh/albedo via a JS byte cache ---------
 * Open once; then poll build/albedo each frame. Each poll kicks the fetch on a miss and returns 0
 * until the bytes are resident (nothing blocks the render loop). FBWorld drives these. */
int  fb_stream_open(const char *base, double lat, double lon, int z);
void fb_stream_set_base(int mode);          /* boot base mode (0 osm, 1 photo) — worker priority hint */
void fb_stream_campos(double lat, double lon);   /* live camera track for the worker's nearest-first pump */
int  fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                     double origin[3], float *err);   /* err = geometric error (m), drives the SSE LOD */
/* Albedo mip PYRAMID (level 0..N packed contiguous, fb_pyramid_bytes(ts) long) for `mode` (0 = /bake/osm,
 * 1 = /bake/photo) into dst. Returns bytes written (>0) resident, 0 pending, -1 the server has no bake
 * for this tile/mode (a real 204 hole). WASM: off-thread via the worker; native: synchronous. */
int  fb_stream_pyramid(int z, uint32_t x, uint32_t y, int mode, int ts, uint8_t *dst);
void fb_stream_close(void);

/* Terrain elevation (m ASL) under (lat,lon) from fb-tiles /elev — the SAME DEM the mesh/HUD use, so
 * JSBSim's ground-reaction floor matches the rendered terrain (crash contract). Returns the ground
 * ASL, or <= -1e8 until a real sample has landed. WASM: async (kicks a fetch, returns the last
 * resolved value — never blocks the frame loop). Native: synchronous curl (block=1), cached per
 * ~tile-cell so a moving aircraft only refetches when it has moved appreciably. */
double fb_stream_ground(double lat, double lon);

/* Night-light list for one tile from fb-tiles /t/lights/z/x/y (see tiles/lights.c wire format:
 * {u16 count,u16 res=0} then count*{u16 x,u16 y (tile-local 0..65535), u8 class, u8 intensity}).
 * Copies up to `cap` bytes into dst; returns bytes written (>=4, the header even when count=0),
 * 0 pending (WASM: fetch in flight), -1 unavailable/too big. Lowest-priority stream (call after
 * terrain + overlay). WASM: async main-thread fetch, in-flight-capped, never blocks the frame loop.
 * Native: synchronous curl behind the byte cache. */
int fb_stream_lights(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap);

/* Decode an image FILE (WASM: embedded MEMFS path e.g. "/moon.jpg"; native: a disk path) to a
 * malloc'd RGBA8 buffer (caller free()s). Returns 1 on success (rgba/w/h set), 0 on any failure. */
int  fb_load_image_file(const char *path, uint8_t **rgba, int *w, int *h);
/* Fetch the concatenated HYG star bands from fb-tiles (`base`/t/stars/{0..}/0/0) into dst (cap bytes,
 * blocking startup fetch). Returns total bytes, 0 if unreachable. */
int  fb_fetch_stars(const char *base, uint8_t *dst, int cap);

#ifdef __cplusplus
}
#endif
#endif
