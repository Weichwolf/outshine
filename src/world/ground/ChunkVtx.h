#ifndef OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#define OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#include "math/Vec2.h"
#include "math/Vec3.h"
#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <stdlib.h>

namespace outshine {

struct ChunkVtx {
  Vec3f pos;
  Vec2f uv;
  Vec3f norm;
};

static_assert(std::is_trivially_copyable_v<ChunkVtx>);
static_assert(std::is_standard_layout_v<ChunkVtx>);
static_assert(sizeof(ChunkVtx) == 8 * sizeof(float),
              "tightly packed: a run of these is a float run");
static_assert(offsetof(ChunkVtx, pos) == 0);
static_assert(offsetof(ChunkVtx, uv) == 3 * sizeof(float));
static_assert(offsetof(ChunkVtx, norm) == 5 * sizeof(float));

constexpr size_t kChunkVtxFloats = sizeof(ChunkVtx) / sizeof(float);

constexpr size_t kChunkVtxUvAt = offsetof(ChunkVtx, uv) / sizeof(float);
constexpr size_t kChunkVtxNormAt = offsetof(ChunkVtx, norm) / sizeof(float);

struct PlainVtx {
  Vec3f pos;
  Vec3f norm;
};

static_assert(std::is_standard_layout_v<PlainVtx>);
static_assert(sizeof(PlainVtx) == 6 * sizeof(float), "tightly packed");
static_assert(offsetof(PlainVtx, norm) == 3 * sizeof(float));

constexpr uint64_t kPlainVertexStrideB = sizeof(PlainVtx);

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

} // namespace outshine
#endif
