/* The tile-streaming C ABI. */
#ifndef TERRAINLOADER_H
#define TERRAINLOADER_H
#include <stdint.h>

#include "ClusterDag.h"

/* A C ABI in name only: the tile mesh crosses it as the DAG the renderer draws, so the cluster type
 * is part of the contract. Every caller is C++ (see the C-ISLAND note in tools/verify_layers.py). */
extern "C" {

/* The finest tile of the caller's LOD ladder and its quads per edge — the surface the renderer draws
 * at the near tier, and therefore the one fb_stream_ground evaluates. The ladder is the world's, so
 * it is handed over once instead of being restated here. */
struct FbGroundSurface { int Z; int Grid; };

/* Open once, then poll each frame: a poll kicks the fetch on a miss and returns 0 until the bytes
 * are resident. Nothing here ever blocks the browser's render loop. */
int  fb_stream_open(const char *base, double lat, double lon, FbGroundSurface surface);
void fb_stream_campos(double lat, double lon);   /* live camera track for the worker's nearest-first pump */
/* THE TILE AS THE RENDERER DRAWS IT: the cluster DAG, not the vertex soup. Both are built where the
 * bytes land — the browser's tile worker, a native worker thread — because the DAG is 3.68 ms of a
 * measured 10.7 ms per tile and a frame thread that pays it drops the frame.
 * `verts`, `idx` and `clusters` are malloc'd; the caller frees all three. A cluster's First/Count is
 * an INDEX range. 0 = not resident yet, poll again. */
int  fb_stream_build(int z, uint32_t x, uint32_t y, int grid, float **verts, int *nverts,
                     uint32_t **idx, int *nidx,
                     outshine::DagCluster **clusters, int *nclusters,
                     double origin[3], float *err);   /* err = geometric error (m), drives the SSE LOD */
/* THE CLUSTER DAG OF A VERTEX SOUP in core/ChunkVtx.h's layout, off the frame thread — the same
 * pool the tile mesh runs in. `id` names the job and is repeated until it is collected; the soup is
 * copied on the first call. `seamAttr` >= 0 names a float whose SIGN marks an attribute seam: two vertices at
 * one position that disagree in it weld as one point and stay two for drawing, which is how a roof
 * cap keeps its crease without reading as a mesh boundary. -1 = no seam.
 * 1 + all three arrays (malloc'd, caller frees), or 0 = not ready. */
int  fb_stream_dag(int id, const float *soup, int nverts, int seamAttr, float **verts, int *nverts_out,
                   uint32_t **idx, int *nidx,
                   outshine::DagCluster **clusters, int *nclusters);

void fb_stream_close(void);

/* Bytes the tile byte caches hold that are reachable from this module. Natively that is both caches.
 * In the browser it is the main thread's fetch caches alone, held as JavaScript typed arrays outside
 * the linear memory; the pool's own caches live in the worker modules and are not counted here. */
double fb_stream_cache_bytes(void);

/* THE DRAWN SURFACE, in metres on the DEM's own datum: the triangle the finest tile of the ladder
 * carries at this position, on that tile's posting indices and along its diagonal. <= -1e8 until
 * every byte that triangle needs has landed. */
double fb_stream_ground(double lat, double lon);

/* Raw Terrarium bytes, ONE tile. The payload is owned by the byte cache — do NOT free. 1 ready,
 * 0 PENDING (retry; never treat as a hole), -1 a real hole. */
int fb_stream_dem(int z, int x, int y, const uint8_t **bytes, int *len);

/* The OSM vector tile behind /t/vector, raw MVT bytes. >0 = length, 0 pending, -1 unavailable or
 * larger than cap. It must never block. */
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
