#include "LightCellFrame.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>

namespace outshine::Render {
std::expected<LightCellFrame, std::string> MakeLightCellFrame(const Vec3 &eye,
                                                              LightCellLookup lookup) {
  if (!std::ranges::all_of(eye, [](double value) { return std::isfinite(value); }) ||
      (lookup != LightCellLookup::Hash && lookup != LightCellLookup::Sorted &&
       lookup != LightCellLookup::Full)) {
    return std::unexpected("light frame has invalid camera or lookup kind");
  }
  LightCellFrame result;
  result.PositionLocal[3] = LightCellFrame::kLookupWidthM;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double cell = std::floor(eye[axis] / LightCellFrame::kPositionWidthM);
    if (cell < -LightCellFrame::kPositionCellLimit || cell >= LightCellFrame::kPositionCellLimit) {
      return std::unexpected("camera exceeds the signed light-position domain");
    }
    result.PositionCell[axis] = static_cast<int32_t>(cell);
    result.PositionLocal[axis] =
        static_cast<float>(eye[axis] - cell * LightCellFrame::kPositionWidthM);
    const double key = std::floor(eye[axis] / LightCellFrame::kLookupWidthM);
    if (key < std::numeric_limits<int32_t>::min() || key > std::numeric_limits<int32_t>::max()) {
      lookup = LightCellLookup::Full;
    } else {
      result.LookupCell[axis] = static_cast<int32_t>(key);
    }
  }
  result.PositionCell[3] = static_cast<int32_t>(lookup);
  return result;
}
}
