#include <expected>
#include "TerrainResidency.h"
#include <utility>

namespace outshine::Render {
bool TerrainResidency::ReusePagesFrom(const TerrainResidency &previous) {
  if (Pages_ || previous.PagesLive_ == 0 || !previous.Pages_ || !previous.Nearest_ ||
      (Device_ != nullptr && Device_ != previous.Device_)) {
    return false;
  }
  Device_ = previous.Device_;
  Pages_ = previous.Pages_;
  Nearest_ = previous.Nearest_;
  Spare_ = previous.Spare_;
  PagesInUse_ = previous.PagesInUse_;
  PagesMade_ = previous.PagesMade_;
  PageLayers_ = previous.PageLayers_;
  PagesLive_ = previous.PagesLive_;
  Patches_.ReusePagesFrom(previous.Patches_);
  ++Generation_;
  return true;
}

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
