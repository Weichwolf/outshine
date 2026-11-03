#include "GroundQuality.h"
#include "GroundRegionDefinition.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
bool Valid(const GroundRegionDefinition &value) {
  const auto &coverage = value.Coverage;
  if (!std::isfinite(coverage.LatitudeDeg) || !std::isfinite(coverage.LongitudeDeg) ||
      !std::isfinite(value.Anchor.LatitudeDeg) || !std::isfinite(value.Anchor.LongitudeDeg) ||
      !std::ranges::all_of(value.EyeM, [](double axis) { return std::isfinite(axis); }) ||
      !std::isfinite(value.YfovRad) || !std::isfinite(value.YMagM) ||
      !std::isfinite(value.ErrorPx) || value.ErrorPx <= 0 || value.HeightPx <= 0 ||
      value.PagesMost == 0 ||
      (value.Quality != GroundQuality::Playable && value.Quality != GroundQuality::Refined)) {
    return false;
  }
  switch (value.Projection) {
    case GroundProjection::Perspective:
      return value.YfovRad > 0 && value.YfovRad < std::numbers::pi;
    case GroundProjection::Orthographic: return value.YMagM > 0;
  }
  return false;
}
}

std::expected<std::vector<uint8_t>, std::string>
EncodeGroundRegionDefinition(const GroundRegionDefinition &definition) {
  if (!Valid(definition)) { return std::unexpected("invalid ground region definition"); }
  constexpr size_t bytesMost = 4096;
  BinaryValueWriter out(bytesMost);
  const auto &coverage = definition.Coverage;
  if (!out(coverage.LatitudeDeg,
           coverage.LongitudeDeg,
           coverage.Zoom,
           coverage.Levels,
           coverage.Grid,
           coverage.PlayableOnly,
           definition.Quality,
           definition.Anchor.LatitudeDeg,
           definition.Anchor.LongitudeDeg,
           definition.EyeM,
           definition.Projection,
           definition.YfovRad,
           definition.YMagM,
           definition.HeightPx,
           definition.ErrorPx,
           definition.PagesMost)) {
    return std::unexpected("ground region parameters exceed their encoding limit");
  }
  if (definition.DeclaredDigest && !out.Text(*definition.DeclaredDigest)) {
    return std::unexpected("declared ground input identity exceeds its encoding limit");
  }
  return std::move(out.Out).TakeBytes();
}
}
