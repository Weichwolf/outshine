#ifndef OUTSHINE_BASE_SPATIAL_STOREDVERTEX_H
#define OUTSHINE_BASE_SPATIAL_STOREDVERTEX_H
#include "math/Octahedral.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <stdlib.h>

namespace outshine {

struct StoredVertex {
  Vec3f pos;

  Vec2f texture;

  uint32_t normWord;

  [[nodiscard]] Vec2f uv() const { return texture; }

  [[nodiscard]] Vec3f norm() const {
    const std::array<float, 3> held = OctUnfolded(UnpackedPair(normWord));
    return Vec3f{{held[0], held[1], held[2]}};
  }

  [[nodiscard]] static StoredVertex
  Of(const Vec3f &placeM, const Vec2f &texture, const Vec3f &facing) {
    return StoredVertex{.pos = placeM,
                        .texture = texture,
                        .normWord = PackedPair(OctFolded({{facing[0], facing[1], facing[2]}}))};
  }
};

static_assert(std::is_trivially_copyable_v<StoredVertex>);
static_assert(std::is_standard_layout_v<StoredVertex>);
static_assert(sizeof(StoredVertex) == 6 * sizeof(float));
static_assert(offsetof(StoredVertex, pos) == 0);
static_assert(offsetof(StoredVertex, texture) == 3 * sizeof(float));
static_assert(offsetof(StoredVertex, normWord) == 5 * sizeof(float));

constexpr size_t kStoredVertexFloats = sizeof(StoredVertex) / sizeof(float);

constexpr size_t kStoredVertexUvAt = offsetof(StoredVertex, texture) / sizeof(float);
constexpr size_t kStoredVertexNormAt = offsetof(StoredVertex, normWord) / sizeof(float);

struct PlainVtx {
  Vec3f pos;
  Vec3f norm;
};

static_assert(std::is_standard_layout_v<PlainVtx>);
static_assert(sizeof(PlainVtx) == 6 * sizeof(float), "tightly packed");
static_assert(offsetof(PlainVtx, norm) == 3 * sizeof(float));

constexpr uint64_t kPlainVertexStrideB = sizeof(PlainVtx);

}
#endif
