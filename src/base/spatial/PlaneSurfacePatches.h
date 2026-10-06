#ifndef OUTSHINE_BASE_SPATIAL_PLANESURFACEPATCHES_H
#define OUTSHINE_BASE_SPATIAL_PLANESURFACEPATCHES_H

#include "math/Vec3.h"

#include <cstddef>
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace outshine {

struct PlaneSurfaceSample {
  uint64_t Surface = 0;
  Vec3 Point, Normal;
};

struct PlaneSurfacePatch {
  uint32_t Left = 0, Top = 0, Right = 0, Bottom = 0;
  size_t Sample = 0;
};

enum class PlaneSurfaceError { InvalidDimensions, InvalidSamples };

[[nodiscard]] std::expected<std::vector<PlaneSurfacePatch>, PlaneSurfaceError>
BuildPlaneSurfacePatches(std::span<const PlaneSurfaceSample> samples,
                         uint32_t width,
                         uint32_t height);

[[nodiscard]] std::optional<std::array<Vec3, 4>> IntersectPlaneSurface(
    const PlaneSurfaceSample &sample, const Vec3 &eye, const std::array<Vec3, 4> &directions);

}
#endif
