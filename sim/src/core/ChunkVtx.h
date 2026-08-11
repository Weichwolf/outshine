/* The vertex LAYOUT of everything that goes through the cluster DAG — terrain and buildings: the one
 * contract between the writer (the mesh build) and the reader (the draw call).
 * Alone in its own header because the main thread needs only the layout,
 * and including the build functions where nothing calls them is -Wunused-function under -Werror.
 *
 * Stride and offsets are DERIVED from the struct and pinned by static_assert: they used to be literals
 * spelled out at the draw call, and getting one wrong does not error — it renders garbage. */
#ifndef CHUNKVTX_H
#define CHUNKVTX_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace outshine {

struct ChunkVtx {
  float pos[3];
  float uv[2];
  float norm[3];
};

/* THE STRIDE OF THIS LAYOUT, in bytes. It sits with the struct so a pipeline descriptor cannot
 * spell a number the writer disagrees with. */
constexpr uint64_t kVertexStrideB = 8 * sizeof(float);

/* THE DECLARED SECOND LAYOUT, for a surface that carries no uv at all. Not a saving: a uv is metres
 * on a surface, and where the fragment is a function of position and normal alone there are no such
 * metres to write down — a zero in that field would be a number meaning nothing, and a tangent
 * derived from it would be worse. The bark is one: its furrow reads a circumferential angle and a
 * height off the position. */
struct PlainVtx {
  float pos[3];
  float norm[3];
};
constexpr uint64_t kPlainVertexStrideB = 6 * sizeof(float);
static_assert(sizeof(PlainVtx) == kPlainVertexStrideB, "vertex must be tightly packed (no padding)");
static_assert(offsetof(PlainVtx, norm) == 12, "aNorm offset");

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
static_assert(sizeof(ChunkVtx) == kVertexStrideB,
              "vertex must be tightly packed (no padding)");
static_assert(offsetof(ChunkVtx, pos) == 0, "aPos offset");
static_assert(offsetof(ChunkVtx, uv) == 12, "aUV offset");
static_assert(offsetof(ChunkVtx, norm) == 20, "aNorm offset");

} // namespace outshine
#endif /* CHUNKVTX_H */
