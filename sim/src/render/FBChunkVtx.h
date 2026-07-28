/* The terrain vertex LAYOUT: the one contract between the writer (the tile worker's mesh build) and
 * the reader (the draw call). Alone in its own header because the main thread needs only the layout,
 * and including the build functions where nothing calls them is -Wunused-function under -Werror.
 *
 * Stride and offsets are DERIVED from the struct and pinned by static_assert: they used to be literals
 * spelled out at the draw call, and getting one wrong does not error — it renders garbage. */
#ifndef FBCHUNKVTX_H
#define FBCHUNKVTX_H
#include <stddef.h>
#include <stdlib.h>

namespace FlightBox::Render {

struct FBChunkVtx {
  float pos[3];
  float uv[2];
  float norm[3];
};

/* Here with the layout because BOTH builders (ENU and ECEF) produce it, and neither should have to
 * include the other just for the struct. */
struct FBChunk {
  FBChunkVtx *verts;
  int nverts;
  float err; /* max |drawn surface - source height| in METRES; drives the LOD.
              * 0 for the irregular-mesh fallback: nothing was decimated there. */
};
inline void FBChunkFree(FBChunk *c) {
  if (!c) return;
  free(c->verts);
  c->verts = 0;
  c->nverts = 0;
  c->err = 0.f;
}
static_assert(sizeof(FBChunkVtx) == 8 * sizeof(float),
              "terrain vertex must be tightly packed (no padding)");
static_assert(offsetof(FBChunkVtx, pos) == 0, "aPos offset");
static_assert(offsetof(FBChunkVtx, uv) == 12, "aUV offset");
static_assert(offsetof(FBChunkVtx, norm) == 20, "aNorm offset");

} // namespace FlightBox::Render
#endif /* FBCHUNKVTX_H */
