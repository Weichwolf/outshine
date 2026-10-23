#include "BuildingField.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {

size_t BuildingField::PhysicalProduct::HeapBytes() const noexcept {
  return CapacityBytes(Prints) + CapacityBytes(SeatSpreadM) + CapacityBytes(AcrossM) + ContactBytes;
}

std::shared_ptr<const BuildingField::PhysicalProduct>
BuildingField::PrepareProduct(uint32_t tile, const Baked &baked) const {
  const auto at = std::ranges::lower_bound(AcceptedTiles_, tile);
  if (at != AcceptedTiles_.end() && *at == tile) {
    const auto &physical = Products_[static_cast<size_t>(at - AcceptedTiles_.begin())].Physical;
    if (std::ranges::equal(physical->Prints, baked.Prints) &&
        std::ranges::equal(physical->SeatSpreadM, baked.SeatSpreadM) &&
        std::ranges::equal(physical->AcrossM, baked.AcrossM)) {
      return physical;
    }
  }
  size_t contactBytes = 0;
  for (const auto &print : baked.Prints) { contactBytes += print.HeapBytes(); }
  return std::make_shared<const PhysicalProduct>(
      PhysicalProduct{.Prints = {baked.Prints.begin(), baked.Prints.end()},
                      .SeatSpreadM = {baked.SeatSpreadM.begin(), baked.SeatSpreadM.end()},
                      .AcrossM = {baked.AcrossM.begin(), baked.AcrossM.end()},
                      .ContactBytes = contactBytes});
}

const BuildingField::TileProduct *BuildingField::ProductForFootprint(size_t index) const noexcept {
  const auto at = std::ranges::upper_bound(
      Products_, index, {}, [](const TileProduct &product) { return product.Prints.First; });
  if (at == Products_.begin()) { return nullptr; }
  const auto &product = *(at - 1);
  return index < product.Prints.First + product.Prints.Count ? &product : nullptr;
}

std::span<const BuildingFootprint> BuildingField::OfTile(int tile) const noexcept {
  if (tile < 0) { return {}; }
  const auto at = std::ranges::lower_bound(AcceptedTiles_, static_cast<uint32_t>(tile));
  if (at == AcceptedTiles_.end() || std::cmp_not_equal(*at, tile)) { return {}; }
  return Products_[static_cast<size_t>(at - AcceptedTiles_.begin())].Physical->Prints;
}

const BuildingGeometry *BuildingField::GeometryOfFootprint(size_t index) const noexcept {
  const auto *product = ProductForFootprint(index);
  return product != nullptr
             ? AcceptedInputs_[static_cast<size_t>(product - Products_.data())].Coordinates.get()
             : nullptr;
}

std::vector<BuildingField::PinnedProduct> BuildingField::PinProducts() const {
  std::vector<PinnedProduct> pinned;
  pinned.reserve(Products_.size());
  for (size_t at = 0; at < Products_.size(); ++at) {
    pinned.push_back({.Physical = Products_[at].Physical,
                      .Coordinates = AcceptedInputs_[at].Coordinates,
                      .FirstFootprint = Products_[at].Prints.First});
  }
  return pinned;
}

size_t BuildingField::PrintBytes() const noexcept {
  size_t bytes = ContactBytes_;
  for (const auto &product : Products_) { bytes += CapacityBytes(product.Physical->Prints); }
  return bytes;
}

size_t BuildingField::MeasurementBytes() const noexcept {
  size_t bytes = 0;
  for (const auto &product : Products_) {
    bytes +=
        CapacityBytes(product.Physical->SeatSpreadM) + CapacityBytes(product.Physical->AcrossM);
  }
  return bytes;
}

size_t BuildingField::HeapBytes(HeapScope scope) const noexcept {
  size_t bytes = CapacityBytes(AcceptedTiles_) + CapacityBytes(AcceptedInputs_) +
                 CapacityBytes(Products_) + Admission_.HeapBytes();
  for (const auto &product : Products_) {
    if (scope == HeapScope::Reachable || product.Physical.use_count() == 1) {
      bytes += sizeof(PhysicalProduct) + product.Physical->HeapBytes();
    }
  }
  for (const AcceptedInput &input : AcceptedInputs_) {
    if (input.Bake.PreparedAssetKey) { bytes += input.Bake.PreparedAssetKey->capacity() + 1u; }
    if (input.Coordinates &&
        (scope == HeapScope::Reachable || input.Coordinates.use_count() == 1)) {
      bytes += input.Coordinates->HeapBytes();
    }
    if (input.Vector) {
      bytes += input.Vector->SourceId.capacity() + input.Vector->Revision.capacity();
    }
    bytes += CapacityBytes(input.Sources) + CapacityBytes(input.Heights.Tiles);
    for (const auto &source : input.Sources) {
      bytes += source.SourceId.capacity() + source.Revision.capacity();
    }
  }
  return bytes;
}

}
