/* THE HEIGHT ORACLE AND THE POOL IT READS THROUGH. What the renderer draws, answered as a number,
 * plus the two bring-up fetches a client needs before there is a world at all.
 *
 * NOT a C ABI any more: every caller is C++, the pool it hands back is an object (world/TilePool.h)
 * and the ground answers with a state. Free functions because the oracle is a service — a footprint,
 * a tree and the eye all ask it, and none of them can reach a World. */
#ifndef TERRAINLOADER_H
#define TERRAINLOADER_H
#include <stdint.h>

#include "GroundSample.h"
#include "TilePool.h"

/* The finest tile of the caller's LOD ladder and its quads per edge — the surface the renderer draws
 * at the near tier, and therefore the one fb_stream_ground evaluates. The ladder is the world's, so
 * it is handed over once instead of being restated here. */
struct FbGroundSurface { int Z; int Grid; };

/* Opens the pool and the oracle over it. The pool's threads are running when this returns. */
int  fb_stream_open(const char *base, double lat, double lon, FbGroundSurface surface);
void fb_stream_close(void);
/* The pool the world streams through, null before fb_stream_open. Borrowed, never owned. */
outshine::World::TilePool *fb_tile_pool(void);

/* THE DRAWN SURFACE, in metres on the DEM's own datum: the triangle the finest tile of the ladder
 * carries at this position, on that tile's posting indices and along its diagonal. Pending until
 * every byte that triangle needs has landed; Hole where the source carries none. */
outshine::GroundSample fb_stream_ground(double lat, double lon);

/* The node spacing of that surface at a latitude, in metres: what a caller must step to land on the
 * next posting instead of re-reading its own triangle. Derived from the surface handed to
 * fb_stream_open, which is where the zoom and the grid are stated. */
double fb_stream_ground_post_m(double latDeg);

/* WASM: an embedded MEMFS path; native: a disk path. RGBA8 out is malloc'd — the caller frees. */
int  fb_load_image_file(const char *path, uint8_t **rgba, int *w, int *h);
/* The concatenated HYG star bands, through the pool. A POLL, because the only thread that could wait
 * for them is the one their fetches complete on. `Bytes` is meaningless while it is Pending, which
 * is why it is not a count with a sentinel in it. */
struct FbStarBands {
  enum class State { Pending, Complete };
  State Where;
  int Bytes;
};
FbStarBands fb_fetch_stars(uint8_t *dst, int cap);

#endif
