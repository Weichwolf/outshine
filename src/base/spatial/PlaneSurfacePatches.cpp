#include "PlaneSurfacePatches.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
namespace {

constexpr double kNormalDifferenceSquared = 1.0e-20;
constexpr double kCoplanarToleranceM = 1.0e-6;

bool SamePlane(const PlaneSurfaceSample &a, const PlaneSurfaceSample &b) {
  if (a.Surface != b.Surface) { return false; }
  const Vec3 difference = a.Normal - b.Normal;
  return Dot(difference, difference) <= kNormalDifferenceSquared &&
         std::abs(Dot(b.Point - a.Point, a.Normal)) <= kCoplanarToleranceM;
}

bool Valid(const PlaneSurfaceSample &sample) {
  const auto finite = [](const Vec3 &value) {
    return std::ranges::all_of(value, [](double coordinate) { return std::isfinite(coordinate); });
  };
  return sample.Surface == 0 ||
         (finite(sample.Point) && finite(sample.Normal) && Dot(sample.Normal, sample.Normal) > 0.0);
}

struct SampleGrid {
  std::span<const PlaneSurfaceSample> Samples;
  uint32_t Width = 0, Height = 0;
};

PlaneSurfacePatch RectangleAt(const SampleGrid &grid, std::span<const uint8_t> used, size_t first) {
  const auto samples = grid.Samples;
  const auto width = grid.Width;
  const auto height = grid.Height;
  const auto x = static_cast<uint32_t>(first % width);
  const auto y = static_cast<uint32_t>(first / width);
  const auto compatible = [&](uint32_t column, uint32_t row) {
    const size_t at = static_cast<size_t>(row) * width + column;
    return used[at] == 0 && SamePlane(samples[first], samples[at]);
  };
  uint32_t right = x + 1;
  while (right < width && compatible(right, y)) { ++right; }
  const auto rowFits = [&](uint32_t row) {
    for (uint32_t column = x; column < right; ++column) {
      if (!compatible(column, row)) { return false; }
    }
    return true;
  };
  uint32_t bottom = y + 1;
  while (bottom < height && rowFits(bottom)) { ++bottom; }
  return {.Left = x, .Top = y, .Right = right, .Bottom = bottom, .Sample = first};
}

}

std::expected<std::vector<PlaneSurfacePatch>, PlaneSurfaceError> BuildPlaneSurfacePatches(
    std::span<const PlaneSurfaceSample> samples, uint32_t width, uint32_t height) {
  const uint64_t count = static_cast<uint64_t>(width) * height;
  if (width == 0 || height == 0 || count > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(PlaneSurfaceError::InvalidDimensions);
  }
  if (samples.size() != count || !std::ranges::all_of(samples, Valid)) {
    return std::unexpected(PlaneSurfaceError::InvalidSamples);
  }
  std::vector<uint8_t> used(samples.size());
  std::vector<PlaneSurfacePatch> patches;
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t first = static_cast<size_t>(y) * width + x;
      if (used[first] != 0 || samples[first].Surface == 0) { continue; }
      const auto patch =
          RectangleAt({.Samples = samples, .Width = width, .Height = height}, used, first);
      const auto right = patch.Right;
      const auto bottom = patch.Bottom;
      for (uint32_t row = y; row < bottom; ++row) {
        const size_t at = static_cast<size_t>(row) * width;
        std::ranges::fill(std::span(used).subspan(at + x, right - x), uint8_t{1});
      }
      patches.push_back({.Left = x, .Top = y, .Right = right, .Bottom = bottom, .Sample = first});
      x = right - 1;
    }
  }
  return patches;
}

std::optional<std::array<Vec3, 4>> IntersectPlaneSurface(const PlaneSurfaceSample &sample,
                                                         const Vec3 &eye,
                                                         const std::array<Vec3, 4> &directions) {
  const double numerator = Dot(sample.Point - eye, sample.Normal);
  std::array<Vec3, 4> corners;
  for (size_t at = 0; at < directions.size(); ++at) {
    const double denominator = Dot(directions[at], sample.Normal);
    if (denominator == 0.0) { return std::nullopt; }
    const double along = numerator / denominator;
    if (!std::isfinite(along) || along <= 0.0) { return std::nullopt; }
    corners[at] = eye + directions[at] * along;
    if (!std::ranges::all_of(corners[at], [](double value) { return std::isfinite(value); })) {
      return std::nullopt;
    }
  }
  return corners;
}

}
