#ifndef OUTSHINE_EXPERIMENT_PATCH_DEPTH_ENVELOPE_H
#define OUTSHINE_EXPERIMENT_PATCH_DEPTH_ENVELOPE_H

#include "fixed_coverage/FixedPatch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string_view>

namespace PatchSelection {
struct ReductionCosts {
  uint64_t CellTests = 0;
  uint64_t CoveredCellVisits = 0;
  uint64_t DepthUpdates = 0;
  uint32_t Empty = 0;
  uint32_t Dominated = 0;
  uint32_t Retained = 0;
};

class DepthEnvelope {
public:
  static constexpr uint32_t Side = 64;
  static constexpr uint64_t StateBytes = uint64_t{Side} * Side * sizeof(float);

  [[nodiscard]] std::expected<bool, std::string_view> Keep(const FixedCoverage::FixedPatch &patch) {
    if (!Valid(patch)) { return std::unexpected("invalid reduction packet/profile"); }
    const auto bounds = Bounds(patch);
    bool covered = false;
    bool changed = false;
    for (uint32_t y = bounds[1]; y < bounds[3]; ++y) {
      for (uint32_t x = bounds[0]; x < bounds[2]; ++x) {
        ++Costs_.CellTests;
        if (!FixedCoverage::FullPixel(patch, x, y)) { continue; }
        covered = true;
        ++Costs_.CoveredCellVisits;
        auto &held = Depth_[size_t{y} * Side + x];
        if (patch.Depth[0] > held) {
          held = patch.Depth[0];
          changed = true;
          ++Costs_.DepthUpdates;
        }
      }
    }
    if (changed) {
      ++Costs_.Retained;
    } else if (covered) {
      ++Costs_.Dominated;
    } else {
      ++Costs_.Empty;
    }
    return changed;
  }

  [[nodiscard]] const ReductionCosts &Costs() const { return Costs_; }

private:
  [[nodiscard]] static bool Valid(const FixedCoverage::FixedPatch &patch) {
    const auto &meta = patch.Meta;
    if (meta[0] < 3 || meta[0] > FixedCoverage::MaximumCorners || meta[1] != Side ||
        meta[2] != Side || meta[3] == 0 || meta[3] > FixedCoverage::PreferredScale ||
        !std::isnormal(patch.Depth[0]) || patch.Depth[0] <= 0 || patch.Depth[0] > 1) {
      return false;
    }
    const auto limit = static_cast<int32_t>(Side * meta[3]);
    for (size_t i = 0; i < meta[0]; ++i) {
      if (patch.Corners[i][0] < 0 || patch.Corners[i][1] < 0 || patch.Corners[i][0] > limit ||
          patch.Corners[i][1] > limit) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static std::array<uint32_t, 4> Bounds(const FixedCoverage::FixedPatch &patch) {
    const auto scale = patch.Meta[3];
    std::array<uint32_t, 4> result{Side * scale, Side * scale, 0, 0};
    for (size_t i = 0; i < patch.Meta[0]; ++i) {
      const auto x = static_cast<uint32_t>(patch.Corners[i][0]);
      const auto y = static_cast<uint32_t>(patch.Corners[i][1]);
      result[0] = std::min(result[0], x);
      result[1] = std::min(result[1], y);
      result[2] = std::max(result[2], x);
      result[3] = std::max(result[3], y);
    }
    result[0] /= scale;
    result[1] /= scale;
    result[2] = (result[2] + scale - 1) / scale;
    result[3] = (result[3] + scale - 1) / scale;
    return result;
  }

  std::array<float, size_t{Side} * Side> Depth_{};
  ReductionCosts Costs_;
};
}
#endif
