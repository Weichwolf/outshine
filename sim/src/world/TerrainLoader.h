/* The tile-streaming C ABI; full contract table: doc/world/terrain.md, Abschnitt 3. */
#ifndef TERRAINLOADER_H
#define TERRAINLOADER_H
#include <stdint.h>

#include "ClusterDag.h"

/* A C ABI in name only: the tile mesh crosses it as the DAG the renderer draws, so the cluster type
 * is part of the contract. Every caller is C++ (see the C-ISLAND note in tools/verify_layers.py). */
extern "C" {

/* Open once, then poll each frame: a poll kicks the fetch on a miss and returns 0 until the bytes
 * are resident. Nothing here ever blocks the browser's render loop. */
int  fb_stream_open(const char *base, double lat, double lon, int z);
void fb_stream_campos(double lat, double lon);   /* live camera track for the worker's nearest-first pump */
/* THE TILE AS THE RENDERER DRAWS IT: the cluster DAG, not the vertex soup. Both are built where the
 * bytes land — the browser's tile worker, a native worker thread — because the DAG is 3.68 ms of a
 * measured 10.7 ms per tile and a frame thread that pays it drops the frame (doc/world/terrain.md).
 * `verts`, `idx` and `clusters` are malloc'd; the caller frees all three. A cluster's First/Count is
 * an INDEX range. 0 = not resident yet, poll again. */
int  fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                     uint32_t **idx, int *nidx,
                     outshine::Render::DagCluster **clusters, int *nclusters,
                     double origin[3], float *err);   /* err = geometric error (m), drives the SSE LOD */
/* THE CLUSTER DAG OF A VERTEX SOUP (pos3 + norm3 + uv2), off the frame thread — the same pool the
 * tile mesh runs in. `id` names the job and is repeated until it is collected; the soup is copied on
 * the first call. `seamAttr` >= 0 names a float whose SIGN marks an attribute seam: two vertices at
 * one position that disagree in it weld as one point and stay two for drawing, which is how a roof
 * cap keeps its crease without reading as a mesh boundary. -1 = no seam.
 * 1 + all three arrays (malloc'd, caller frees), or 0 = not ready. */
int  fb_stream_dag(int id, const float *soup, int nverts, int seamAttr, float **verts, int *nverts_out,
                   uint32_t **idx, int *nidx,
                   outshine::Render::DagCluster **clusters, int *nclusters);

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

/* The OSM vector tile behind /t/vector, raw MVT bytes. >0 = length, 0 pending, -1 unavailable or
 * larger than cap. Same shape as fb_stream_lights and for the same reason: it must never block. */
int fb_stream_vector(int z, uint32_t x, uint32_t y, uint8_t *dst, int cap);

/* WASM: an embedded MEMFS path; native: a disk path. RGBA8 out is malloc'd — the caller frees. */
int  fb_load_image_file(const char *path, uint8_t **rgba, int *w, int *h);
/* Blocking startup fetch of the concatenated HYG star bands. */
int  fb_fetch_stars(const char *base, uint8_t *dst, int cap);
/* Blocking text GET, url absolute or origin-relative (the WASM boot mission comes off fb-sim's OWN
 * web/ mount, not fb-tiles). dst is NUL-terminated within cap. */
int  fb_fetch_text(const char *url, char *dst, int cap);

}
#endif
