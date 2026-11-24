#include "SceneResources.h"
#include "GeometryRenderer.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
bool SceneResources::HasPrism(PrismHandle which) const noexcept {
  return which.Slot < PrismSources_.size() &&
         PrismSources_[which.Slot].State.Matches(which.Generation);
}

Native::Result<PrismHandle> SceneResources::PlacePrismGrid(GeometryRenderer &geometry,
                                                           std::shared_ptr<const PrismGrid> source,
                                                           std::span<const PrismInstance> instances,
                                                           const MaterialRegistration &material) {
  return RegisterPrism(geometry,
                       {.State = {.Occupied = true},
                        .Source = std::move(source),
                        .Instances = {instances.begin(), instances.end()},
                        .Material = material,
                        .Convex = {}});
}

Native::Result<PrismHandle>
SceneResources::PlaceConvexPrisms(GeometryRenderer &geometry,
                                  std::shared_ptr<const std::vector<ConvexPrism>> source,
                                  const MaterialRegistration &material) {
  if (!source) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "convex prism source is missing"});
  }
  return RegisterPrism(geometry,
                       {.State = {.Occupied = true},
                        .Source = {},
                        .Instances = {},
                        .Material = material,
                        .Convex = std::move(source)});
}

Native::Result<PrismHandle> SceneResources::RegisterPrism(GeometryRenderer &geometry,
                                                          PrismSource held) {
  if (FirstFreePrism_ == kNoResourceSlot && PrismSources_.size() >= kNoResourceSlot) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Unsupported,
                                         .Message = "prism source slots exceed native indices"});
  }
  if (!HasMaterialRegistration(held.Material)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism material has no matching live source"});
  }
  if (auto restored = RestorePrism(geometry, held); !restored) {
    return std::unexpected(std::move(restored.error()));
  }
  uint32_t slot = FirstFreePrism_;
  if (slot != kNoResourceSlot) {
    held.State.Generation = PrismSources_[slot].State.Generation;
    FirstFreePrism_ = PrismSources_[slot].State.NextFree;
    PrismSources_[slot] = std::move(held);
  } else {
    slot = static_cast<uint32_t>(PrismSources_.size());
    PrismSources_.push_back(std::move(held));
  }
  return PrismHandle{.Slot = slot, .Generation = PrismSources_[slot].State.Generation};
}

Native::Result<void> SceneResources::RestorePrism(GeometryRenderer &geometry, PrismSource &held) {
  auto placed = held.Convex
                    ? geometry.PlaceConvexPrisms(*held.Convex, held.Material.First)
                    : geometry.PlacePrismGrid(held.Source, held.Instances, held.Material.First);
  if (!placed) { return std::unexpected(std::move(placed.error())); }
  held.Resident = *placed;
  return {};
}

Native::Result<void> SceneResources::ReplacePrismGrid(GeometryRenderer &geometry,
                                                      PrismHandle which,
                                                      std::shared_ptr<const PrismGrid> source,
                                                      std::span<const PrismInstance> instances,
                                                      const MaterialRegistration &material) {
  if (!HasPrism(which) || PrismSources_[which.Slot].Convex) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism handle names no live grid"});
  }
  auto &held = PrismSources_[which.Slot];
  if (held.Source == source && held.Material.First == material.First &&
      held.Material.Source == material.Source && held.Resident != 0) {
    return SetPrismGridInstances(geometry, which, instances);
  }
  if (!HasMaterialRegistration(material)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism material has no matching live source"});
  }
  auto placed = geometry.PlacePrismGrid(source, instances, material.First);
  if (!placed) { return std::unexpected(placed.error()); }
  if (held.Resident != 0) { geometry.ReleasePrism(held.Resident); }
  held.Resident = *placed;
  held.Source = std::move(source);
  held.Material = material;
  held.Instances.assign(instances.begin(), instances.end());
  return {};
}

Native::Result<void> SceneResources::SetPrismGridInstances(
    GeometryRenderer &geometry, PrismHandle which, std::span<const PrismInstance> instances) {
  if (!HasPrism(which) || PrismSources_[which.Slot].Convex) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism handle names no live grid"});
  }
  auto &held = PrismSources_[which.Slot];
  if (held.Resident == 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "prism source awaits native restoration"});
  }
  const auto updated = geometry.SetPrismGridInstances(held.Resident, instances);
  if (!updated) { return updated; }
  held.Instances.assign(instances.begin(), instances.end());
  return {};
}

void SceneResources::ReleasePrism(GeometryRenderer &geometry, PrismHandle which) {
  if (!HasPrism(which)) { return; }
  auto &held = PrismSources_[which.Slot];
  if (held.Resident != 0) { geometry.ReleasePrism(held.Resident); }
  auto state = held.State;
  if (state.Release()) {
    state.NextFree = FirstFreePrism_;
    FirstFreePrism_ = which.Slot;
  }
  held = {.State = state, .Source = {}, .Instances = {}, .Material = {}, .Convex = {}};
}

bool SceneResources::CanReusePrismResidents(const PrismResidency &residency) const {
  const auto products = residency.Products();
  if (products.size() > PrismSources_.size()) { return false; }
  std::vector<uint64_t> ids;
  ids.reserve(products.size());
  for (const auto &grid : PrismSources_) {
    if (grid.State.Occupied) { ids.push_back(grid.Resident); }
  }
  std::ranges::sort(ids);
  return std::ranges::equal(ids, products, {}, std::identity{}, &PrismProduct::Id);
}

void SceneResources::CopyPrismSourcesFrom(const SceneResources &source,
                                          PieceSources pieces,
                                          bool reusePrismGrids) {
  FirstPrismToRestore_ = 0;
  if (pieces == PieceSources::Omit) {
    PrismSources_.clear();
    FirstFreePrism_ = kNoResourceSlot;
    return;
  }
  PrismSources_ = source.PrismSources_;
  FirstFreePrism_ = source.FirstFreePrism_;
  if (reusePrismGrids) {
    FirstPrismToRestore_ = PrismSources_.size();
    return;
  }
  for (auto &held : PrismSources_) { held.Resident = 0; }
}

std::expected<bool, std::string> SceneResources::AdvancePrismRestore(GeometryRenderer &geometry,
                                                                     size_t &nextGrid,
                                                                     size_t gridsMost) {
  nextGrid = std::max(nextGrid, FirstPrismToRestore_);
  size_t visited = 0;
  while (nextGrid < PrismSources_.size() && visited < gridsMost) {
    auto &held = PrismSources_[nextGrid];
    if (held.State.Occupied && held.Resident == 0) {
      auto placed = RestorePrism(geometry, held);
      if (!placed) {
        if (placed.error().Kind == Native::ErrorKind::Busy) { return false; }
        return std::unexpected(std::move(placed.error().Message));
      }
    }
    ++nextGrid;
    ++visited;
  }
  return nextGrid == PrismSources_.size();
}
}
