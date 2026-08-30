#ifndef OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#define OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace outshine {

struct ChunkVtx {
  float pos[3];
  float uv[2];
  float norm[3];
};

constexpr uint64_t kVertexStrideB = 8 * sizeof(float);

struct PlainVtx {
  float pos[3];
  float norm[3];
};

constexpr uint64_t kPlainVertexStrideB = 6 * sizeof(float);
static_assert(sizeof(PlainVtx) == kPlainVertexStrideB,
              "vertex must be tightly packed (no padding)");
static_assert(offsetof(PlainVtx, norm) == 12, "aNorm offset");

struct Chunk {
  ChunkVtx *verts;
  int nverts;
  int gridverts;
  float err;
};

inline void ChunkFree(Chunk *c) {
  if (!c) { return; }
  free(c->verts);
  c->verts = 0;
  c->nverts = 0;
  c->gridverts = 0;
  c->err = 0.f;
}

static_assert(sizeof(ChunkVtx) == kVertexStrideB, "vertex must be tightly packed (no padding)");
static_assert(offsetof(ChunkVtx, pos) == 0, "aPos offset");
static_assert(offsetof(ChunkVtx, uv) == 12, "aUV offset");
static_assert(offsetof(ChunkVtx, norm) == 20, "aNorm offset");

}
#endif
