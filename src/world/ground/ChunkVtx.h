#ifndef OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#define OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#include "math/Vec2.h"
#include "math/Vec3.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace outshine {

constexpr size_t kChunkVtxBytes = 20;

struct ChunkVtx {
  Vec3f pos;
  Vec2f uv;
  Vec3f norm;
};

constexpr uint64_t kVertexStrideB = 8 * sizeof(float);

struct PlainVtx {
  Vec3f pos;
  Vec3f norm;
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
  if (c == nullptr) { return; }
  free(c->verts);
  c->verts = nullptr;
  c->nverts = 0;
  c->gridverts = 0;
  c->err = 0.f;
}

static_assert(sizeof(ChunkVtx) == kVertexStrideB, "vertex must be tightly packed (no padding)");
static_assert(offsetof(ChunkVtx, pos) == 0, "aPos offset");
static_assert(offsetof(ChunkVtx, uv) == 12, "aUV offset");
static_assert(offsetof(ChunkVtx, norm) == kChunkVtxBytes, "aNorm offset");

} // namespace outshine
#endif
