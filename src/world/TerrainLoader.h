/* THE HEIGHT ORACLE AND THE POOL IT READS THROUGH. What the renderer draws, answered as a number,
 * plus the two bring-up fetches a client needs before there is a world at all.
 *
 * NOT a C ABI any more: every caller is C++, the pool it hands back is an object (world/TilePool.h)
 * and the ground answers with a state. Free functions because the oracle is a service — a footprint,
 * a tree and the eye all ask it, and none of them can reach a World.
 *
 * NOT RE-ENTRANT, and that is a property of the oracle rather than of any caller: its tile slots are
 * a bare LRU whose clock is advanced by every read, so two threads reading it at once race on the
 * clock and on the eviction it decides. One thread asks. The tile POOL under it is thread-safe and
 * is what the workers use. */
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

class FbGroundBlock;
/* ONE TILE OF THE DRAWN SURFACE, HANDED OUT WHOLE. Same heights and the same triangle as
 * fb_stream_ground; what it does not repeat is FINDING the tile. A caller laying a lattice over one
 * tile pays the slot scan, the wrap and the stitch once instead of once per sample.
 *
 * `z` must be the surface's own zoom (fb_stream_open) — a block of any other zoom does not exist. */
FbGroundBlock fb_stream_ground_block(int z, long x, long y);

class FbGroundBlock {
public:
  enum class State { Resolved, Pending, Missing };

  [[nodiscard]] State Where() const noexcept { return Where_; }

  /* ONE PARALLEL OF THE SURFACE, `count` heights in metres on the DEM's datum, equally spaced in
   * longitude from `lonFromDeg`. A row and not a point because that is what the tile's own frame
   * is cheap in: its x is linear in longitude and its y depends on latitude alone, so a sweep pays
   * two transcendentals per ROW here where a per-point call pays two per sample.
   *
   * A position outside this tile reads the tile's own edge: a block is one tile by construction and
   * a neighbour's heights are not in it, so the alternative would be a silent second tile fetch
   * inside the loop this exists to keep out of one. Resolved only. */
  void AslMRow(double latDeg, double lonFromDeg, double lonStepDeg, int count,
               double *out) const noexcept;

private:
  friend FbGroundBlock fb_stream_ground_block(int z, long x, long y);

  /* The oracle's own slot, valid until the next call into the oracle — which may evict this tile.
   * A block is read out in one loop and never held. */
  const float *Nodes_ = nullptr;
  long X_ = 0, Y_ = 0;
  int Zoom_ = 0, Side_ = 0;
  uint32_t Postings_ = 0;
  State Where_ = State::Missing;
};

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
