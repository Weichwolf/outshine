#include "CloudFrame.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>

namespace outshine::Generators::Weather {
namespace {
constexpr double kCellM = 1400.0;
constexpr double kDetailFrequency = 3.1;
constexpr double kMaximumExactCell = 0x1p52;

bool ValidLayer(const CloudLayer &layer) noexcept {
  if (!std::isfinite(layer.RadiusM) || layer.RadiusM <= 0.0 || !std::isfinite(layer.BaseM) ||
      layer.BaseM < 0.0 || !std::isfinite(layer.ThicknessM) || layer.ThicknessM <= 0.0 ||
      !std::isfinite(layer.ExtinctionPerM) || layer.ExtinctionPerM < 0.0 ||
      !std::isfinite(layer.Cover) || layer.Cover < 0.0 || layer.Cover > 1.0 ||
      (layer.Profile != CloudProfile::Stratus && layer.Profile != CloudProfile::CumulusV1)) {
    return false;
  }
  return std::ranges::all_of(layer.WindMs, [](double value) { return std::isfinite(value); });
}

bool Packable(double value) noexcept {
  return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
}

struct LatticeCoordinate {
  uint32_t Base;
  float Fraction;
};

std::optional<LatticeCoordinate> Split(double offset) noexcept {
  if (!std::isfinite(offset) || std::abs(offset) > kMaximumExactCell) { return std::nullopt; }
  const double cell = std::floor(offset);
  LatticeCoordinate result{.Base = static_cast<uint32_t>(static_cast<int64_t>(cell)),
                           .Fraction = static_cast<float>(offset - cell)};
  if (result.Fraction == 1.0f) {
    ++result.Base;
    result.Fraction = 0.0f;
  }
  return result;
}
}

std::expected<CloudFrame, CloudFrameFailure>
MakeCloudFrame(const CloudLayer &layer, double phaseS, const Vec3 &coordinateOriginM) noexcept {
  if (!ValidLayer(layer)) { return std::unexpected(CloudFrameFailure::InvalidLayer); }
  if (!std::isfinite(phaseS)) { return std::unexpected(CloudFrameFailure::InvalidPhase); }
  CloudFrame result;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(coordinateOriginM[axis])) {
      return std::unexpected(CloudFrameFailure::InvalidOrigin);
    }
    if (!Packable(coordinateOriginM[axis])) {
      return std::unexpected(CloudFrameFailure::Unrepresentable);
    }
    result.CoordinateOrigin[axis] = static_cast<float>(coordinateOriginM[axis]);
    const double offset =
        (static_cast<double>(result.CoordinateOrigin[axis]) - layer.WindMs[axis] * phaseS) / kCellM;
    const auto broad = Split(offset);
    const auto detail = Split(offset * kDetailFrequency);
    if (!broad || !detail) { return std::unexpected(CloudFrameFailure::Unrepresentable); }
    result.BroadBase[axis] = broad->Base;
    result.BroadFraction[axis] = broad->Fraction;
    result.DetailBase[axis] = detail->Base;
    result.DetailFraction[axis] = detail->Fraction;
  }
  if (!Packable(layer.RadiusM) || !Packable(layer.BaseM) || !Packable(layer.ThicknessM) ||
      !Packable(layer.ExtinctionPerM)) {
    return std::unexpected(CloudFrameFailure::Unrepresentable);
  }
  result.Shell = {static_cast<float>(layer.RadiusM),
                  static_cast<float>(layer.BaseM),
                  static_cast<float>(layer.ThicknessM),
                  static_cast<float>(layer.ExtinctionPerM)};
  if (result.Shell[0] <= 0.0f || result.Shell[2] <= 0.0f) {
    return std::unexpected(CloudFrameFailure::Unrepresentable);
  }
  result.Field = {static_cast<float>(layer.Cover),
                  static_cast<float>(static_cast<uint32_t>(layer.Profile)),
                  0.0f,
                  0.0f};
  return result;
}

}
