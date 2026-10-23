#include "SceneResources.h"
#include "GeometryRenderer.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
bool SceneResources::HasPrismGrid(PrismGridHandle which) const noexcept {
  return which.Slot < PrismGrids_.size() && PrismGrids_[which.Slot].State.Matches(which.Generation);
}

Native::Result<PrismGridHandle>
SceneResources::PlacePrismGrid(GeometryRenderer &subjects,
                               std::shared_ptr<const PrismGrid> source,
                               std::span<const PrismInstance> instances,
                               uint32_t material) {
  if (FirstFreePrismGrid_ == kNoResourceSlot && PrismGrids_.size() >= kNoResourceSlot) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Unsupported,
                                         .Message = "prism source slots exceed native indices"});
  }
  auto placed = subjects.PlacePrismGrid(source, instances, material);
  if (!placed) { return std::unexpected(placed.error()); }
  PrismSource held{.State = {.Occupied = true},
                   .Source = std::move(source),
                   .Instances = {instances.begin(), instances.end()},
                   .Material = material,
                   .Resident = *placed};
  uint32_t slot = FirstFreePrismGrid_;
  if (slot != kNoResourceSlot) {
    held.State.Generation = PrismGrids_[slot].State.Generation;
    FirstFreePrismGrid_ = PrismGrids_[slot].State.NextFree;
    PrismGrids_[slot] = std::move(held);
  } else {
    slot = static_cast<uint32_t>(PrismGrids_.size());
    PrismGrids_.push_back(std::move(held));
  }
  return PrismGridHandle{.Slot = slot, .Generation = PrismGrids_[slot].State.Generation};
}

Native::Result<void> SceneResources::ReplacePrismGrid(GeometryRenderer &subjects,
                                                      PrismGridHandle which,
                                                      std::shared_ptr<const PrismGrid> source,
                                                      std::span<const PrismInstance> instances,
                                                      uint32_t material) {
  if (!HasPrismGrid(which)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism handle names no live source"});
  }
  auto &held = PrismGrids_[which.Slot];
  if (held.Source == source && held.Material == material && held.Resident != 0) {
    return SetPrismGridInstances(subjects, which, instances);
  }
  auto placed = subjects.PlacePrismGrid(source, instances, material);
  if (!placed) { return std::unexpected(placed.error()); }
  if (held.Resident != 0) { subjects.ReleasePrismGrid(held.Resident); }
  held.Resident = *placed;
  held.Source = std::move(source);
  held.Material = material;
  held.Instances.assign(instances.begin(), instances.end());
  return {};
}

Native::Result<void> SceneResources::SetPrismGridInstances(
    GeometryRenderer &subjects, PrismGridHandle which, std::span<const PrismInstance> instances) {
  if (!HasPrismGrid(which)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism handle names no live source"});
  }
  auto &held = PrismGrids_[which.Slot];
  if (held.Resident == 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "prism source awaits native restoration"});
  }
  const auto updated = subjects.SetPrismGridInstances(held.Resident, instances);
  if (!updated) { return updated; }
  held.Instances.assign(instances.begin(), instances.end());
  return {};
}

void SceneResources::ReleasePrismGrid(GeometryRenderer &subjects, PrismGridHandle which) {
  if (!HasPrismGrid(which)) { return; }
  auto &held = PrismGrids_[which.Slot];
  if (held.Resident != 0) { subjects.ReleasePrismGrid(held.Resident); }
  auto state = held.State;
  if (state.Release()) {
    state.NextFree = FirstFreePrismGrid_;
    FirstFreePrismGrid_ = which.Slot;
  }
  held = {.State = state, .Source = {}, .Instances = {}};
}

void SceneResources::CopyPrismSourcesFrom(const SceneResources &source, PieceSources pieces) {
  if (pieces == PieceSources::Omit) {
    PrismGrids_.clear();
    FirstFreePrismGrid_ = kNoResourceSlot;
    return;
  }
  PrismGrids_ = source.PrismGrids_;
  FirstFreePrismGrid_ = source.FirstFreePrismGrid_;
  for (auto &held : PrismGrids_) { held.Resident = 0; }
}

std::expected<bool, std::string> SceneResources::AdvancePrismRestore(GeometryRenderer &subjects,
                                                                     size_t &nextGrid,
                                                                     size_t gridsMost) {
  size_t visited = 0;
  while (nextGrid < PrismGrids_.size() && visited < gridsMost) {
    auto &held = PrismGrids_[nextGrid];
    if (held.State.Occupied && held.Resident == 0) {
      auto placed = subjects.PlacePrismGrid(held.Source, held.Instances, held.Material);
      if (!placed) {
        if (placed.error().Kind == Native::ErrorKind::Busy) { return false; }
        return std::unexpected(std::move(placed.error().Message));
      }
      held.Resident = *placed;
    }
    ++nextGrid;
    ++visited;
  }
  return nextGrid == PrismGrids_.size();
}
}
