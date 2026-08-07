/* The terrain vertex LAYOUT: the one contract between the writer (the tile worker's mesh build) and
 * the reader (the draw call). Alone in its own header because the main thread needs only the layout,
 * and including the build functions where nothing calls them is -Wunused-function under -Werror.
 *
 * Stride and offsets are DERIVED from the struct and pinned by static_assert: they used to be literals
 * spelled out at the draw call, and getting one wrong does not error — it renders garbage. */
#ifndef CHUNKVTX_H
#define CHUNKVTX_H
#include <stddef.h>
#include <stdlib.h>

namespace outshine::Render {

struct ChunkVtx {
  float pos[3];
  float uv[2];
  float norm[3];
};

/* Here with the layout because BOTH builders (ENU and ECEF) produce it, and neither should have to
 * include the other just for the struct. */
struct Chunk {
  ChunkVtx *verts;
  int nverts;
  int gridverts; /* the leading run that is the height-field grid; the rest is the skirt curtain.
                  * The cluster DAG may only touch the grid — a skirt triangle shares the tile's
                  * boundary edge, which would stop that edge counting as a boundary and let the
                  * simplifier move it, and THAT edge is the seam against the neighbouring tile. */
  float err; /* max |drawn surface - source height| in METRES; drives the LOD.
              * 0 for the irregular-mesh fallback: nothing was decimated there. */
};
inline void ChunkFree(Chunk *c) {
  if (!c) return;
  free(c->verts);
  c->verts = 0;
  c->nverts = 0;
  c->gridverts = 0;
  c->err = 0.f;
}
static_assert(sizeof(ChunkVtx) == 8 * sizeof(float),
              "terrain vertex must be tightly packed (no padding)");
static_assert(offsetof(ChunkVtx, pos) == 0, "aPos offset");
static_assert(offsetof(ChunkVtx, uv) == 12, "aUV offset");
static_assert(offsetof(ChunkVtx, norm) == 20, "aNorm offset");

} // namespace outshine::Render
#endif /* CHUNKVTX_H */
