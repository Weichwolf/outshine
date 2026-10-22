#ifndef OUTSHINE_EXPERIMENT_FIXED_COVERAGE_PATCH_H
#define OUTSHINE_EXPERIMENT_FIXED_COVERAGE_PATCH_H

#include "Exact.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <vector>

namespace FixedCoverage {
constexpr uint32_t MaximumCoordinate = 16384;
constexpr uint32_t MaximumCorners = 8;
constexpr uint32_t PreferredScale = 8;
static_assert(uint64_t{2} * MaximumCoordinate * MaximumCoordinate +
                  uint64_t{2} * MaximumCoordinate * PreferredScale <=
              std::numeric_limits<int32_t>::max());

struct SourceQuad {
  std::array<std::array<float, 4>, 4> Points;
  bool Opaque = false;
  bool Complete = false;
};

struct ExactPoint {
  Exact X;
  Exact Y;
  bool operator==(const ExactPoint &) const = default;
};

struct Projected {
  std::array<ExactPoint, 4> Corners;
  Exact FarDepth;
  int SourceWindowOrientation = 0;
};

struct alignas(16) FixedPatch {
  std::array<std::array<int32_t, 4>, MaximumCorners> Corners{};
  std::array<uint32_t, 4> Meta{};
  std::array<float, 4> Depth{};
};

constexpr size_t UniformBytes = 160;
static_assert(sizeof(FixedPatch) == UniformBytes && alignof(FixedPatch) == 16);
static_assert(offsetof(FixedPatch, Meta) == size_t{MaximumCorners} * 4 * sizeof(int32_t));
static_assert(offsetof(FixedPatch, Depth) == offsetof(FixedPatch, Meta) + 4 * sizeof(uint32_t));

enum class Unknown {
  NonOpaque,
  Incomplete,
  InvalidProfile,
  ClipCrossing,
  Nonfinite,
  Nonconvex,
  Outside,
  Quantization,
  NoDepth
};

[[nodiscard]] Exact Edge(const ExactPoint &a, const ExactPoint &b, const ExactPoint &point);
[[nodiscard]] bool Inside(const std::array<ExactPoint, 4> &corners, const ExactPoint &point);
[[nodiscard]] std::expected<Projected, Unknown>
Project(const SourceQuad &source, uint32_t width, uint32_t height);
[[nodiscard]] std::expected<FixedPatch, Unknown>
Prepare(const SourceQuad &source, uint32_t width, uint32_t height);
[[nodiscard]] bool FullPixel(const FixedPatch &patch, uint32_t x, uint32_t y);
}
#endif
