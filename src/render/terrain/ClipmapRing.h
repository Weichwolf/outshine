#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPRING_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPRING_H

#include "ClipmapHeightCache.h"
#include "Vec2.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace outshine::Render {
struct alignas(16) ClipmapRing {
#define CLIPMAP_FLOAT4(name) alignas(16) std::array<float, 4>(name){};
#define CLIPMAP_INT4(name) alignas(16) std::array<int32_t, 4>(name){};
#include "ClipmapRing.inc"
#undef CLIPMAP_FLOAT4
#undef CLIPMAP_INT4
};

static_assert(sizeof(ClipmapRing) == size_t{7} * 16);
static_assert(alignof(ClipmapRing) == 16);
static_assert(offsetof(ClipmapRing, CentreStep) == 0);
static_assert(offsetof(ClipmapRing, RelativeCentres) == 16);
static_assert(offsetof(ClipmapRing, Morph) == 32);
static_assert(offsetof(ClipmapRing, PreviousMorph) == size_t{3} * 16);
static_assert(offsetof(ClipmapRing, HeightAddress) == 64);
static_assert(offsetof(ClipmapRing, NormalAddress) == size_t{5} * 16);
static_assert(offsetof(ClipmapRing, Offsets) == size_t{6} * 16);

struct ClipmapView {
  Vec2 Camera{};
  Vec2 PreviousCamera{};
  Vec3 Shift{};
  Vec3 PreviousShift{};
};

enum class ClipmapRingError { IncompatibleGrids, Unpublished, InvalidView };

[[nodiscard]] std::expected<std::vector<ClipmapRing>, ClipmapRingError>
MakeClipmapRings(const ClipmapHeightCache &heights,
                 const ClipmapHeightCache &normalHeights,
                 const ClipmapView &view);
}
#endif
