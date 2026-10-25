#include <expected>
#include "TerrainResidency.h"
#include <utility>

namespace outshine::Render {
Native::Result<void> TerrainResidency::ReuseInputsFrom(const TerrainResidency &previous,
                                                       Native::Device &device) {
  if ((Device_ != nullptr && Device_ != &device) ||
      (previous.Device_ != nullptr && previous.Device_ != &device)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground geometry belongs to another device"});
  }
  if (Grid_ || UniformGrid_ || Index_ || PatchGrid_ || PatchIndex_ || PatchFractions_ ||
      !previous.Grid_ || !previous.UniformGrid_ || !previous.Index_ || !previous.PatchGrid_ ||
      !previous.PatchIndex_ || !previous.PatchFractions_) {
    return {};
  }
  Grid_ = previous.Grid_;
  UniformGrid_ = previous.UniformGrid_;
  Index_ = previous.Index_;
  PatchGrid_ = previous.PatchGrid_;
  PatchIndex_ = previous.PatchIndex_;
  PatchFractions_ = previous.PatchFractions_;
  Patches_.Fractions(previous.Patches_.Fractions());
  Device_ = &device;
  ++Generation_;
  return {};
}

Native::Result<void> TerrainResidency::StandsOn(Native::Device &device) {
  if (Device_ != nullptr && Device_ != &device) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground resources belong to another device"});
  }
  Device_ = &device;
  if (!Grid_) {
    const auto grid = BuildGrid({}, Grid_);
    if (!grid) { return grid; }
  }
  if (!UniformGrid_) {
    const auto grid = BuildGrid({}, UniformGrid_);
    if (!grid) { return grid; }
  }
  if (!PatchGrid_) {
    const auto grid = BuildPatchGrid();
    if (!grid) { return grid; }
  }
  return {};
}

}
