#ifndef OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#define OUTSHINE_WORLD_GROUND_CHUNKVTX_H
#include "math/Octahedral.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <stdlib.h>

namespace outshine {

struct ChunkVtx {
  Vec3f pos;

  uint32_t uvWord;

  uint32_t normWord;

  static constexpr float kUvSpan = 4.0f;

  [[nodiscard]] Vec2f uv() const {
    const std::array<float, 2> held = UnpackedPair(uvWord);
    return Vec2f{{held[0] * kUvSpan, held[1] * kUvSpan}};
  }

  [[nodiscard]] Vec3f norm() const {
    const std::array<float, 3> held = OctUnfolded(UnpackedPair(normWord));
    return Vec3f{{held[0], held[1], held[2]}};
  }

  [[nodiscard]] static ChunkVtx Of(const Vec3f &placeM, const Vec2f &texture, const Vec3f &facing) {
    return ChunkVtx{.pos = placeM,
                    .uvWord = PackedPair({{texture[0] / kUvSpan, texture[1] / kUvSpan}}),
                    .normWord = PackedPair(OctFolded({{facing[0], facing[1], facing[2]}}))};
  }
};

static_assert(std::is_trivially_copyable_v<ChunkVtx>);
static_assert(std::is_standard_layout_v<ChunkVtx>);
static_assert(sizeof(ChunkVtx) == 5 * sizeof(float),
              "three floats of position and two packed words -- 20 bytes, what Filament, Metal and "
              "Unreal all store, against the 32 this held before");
static_assert(offsetof(ChunkVtx, pos) == 0);
static_assert(offsetof(ChunkVtx, uvWord) == 3 * sizeof(float));
static_assert(offsetof(ChunkVtx, normWord) == 4 * sizeof(float));

constexpr size_t kChunkVtxFloats = sizeof(ChunkVtx) / sizeof(float);

constexpr size_t kChunkVtxUvAt = offsetof(ChunkVtx, uvWord) / sizeof(float);
constexpr size_t kChunkVtxNormAt = offsetof(ChunkVtx, normWord) / sizeof(float);

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
