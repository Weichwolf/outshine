#include "BuildingField.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace outshine::Ground {

BuildingField BuildingField::SnapshotAccepted() const {
  BuildingField copy = *this;
  const size_t released = copy.Mark_.ReleaseUnaccepted(copy.AcceptedTiles_);
  copy.Taken_ -= released;
  assert(copy.Taken_ == copy.Accepted_);
  return copy;
}

void BuildingField::ResetDerived() {
  ++Revision_;
  Prints_.clear();
  AcceptedTiles_.clear();
  TrianglesHanded_ = 0;
  Taken_ = Accepted_ = 0;
  ByTile_ = {};
  Mark_ = {};
  OsmHeights_ = DefaultHeights_ = Fronted_ = 0;
  SeatSpread_.clear();
  Across_.clear();
}

void BuildingField::AnchorAt(const Vec3 &ecef) {
  assert(Prints_.empty());
  for (int c = 0; c < 3; c++) { Anchor_[c] = ecef[c]; }
  Anchored_ = true;
}

std::optional<TileWatermark::Next>
BuildingField::Next(const OsmField &field,
                    const std::function<bool(FeatureRun)> &groundStands,
                    size_t candidatesMost) {
  assert(Anchored_);
  const std::span<const OsmField::Feature> feats = field.Features();
  if (Mark_.Done(feats)) { return std::nullopt; }
  const TileWatermark::Next next = Mark_.Ask(
      feats,
      field.Tiles(),
      {.CentreX = field.CentreX(),
       .CentreY = field.CentreY(),
       .Rings = kEveryRing,
       .CandidatesMost = candidatesMost},
      [&groundStands](size_t from, size_t to) { return groundStands({.From = from, .To = to}); });
  if (!next.Found) { return std::nullopt; }
  return next;
}

void BuildingField::PreparesAcceptances(AcceptanceCapacity capacity) {
  Prints_.reserve(Prints_.size() + capacity.Prints);
  SeatSpread_.reserve(SeatSpread_.size() + capacity.Spread);
  Across_.reserve(Across_.size() + capacity.Across);
  AcceptedTiles_.reserve(AcceptedTiles_.size() + capacity.Tiles);
  ByTile_.Prepare(capacity.LargestTile);
}

BuildingField::PendingAcceptance BuildingField::PrepareAcceptance(uint32_t tile,
                                                                  const Baked &baked) {
  return {this, tile, baked};
}

void BuildingField::CommitAcceptance(PendingAcceptance pending,
                                     const OsmField &field,
                                     const Baked &baked) noexcept {
  assert(pending.Owner_ == this && pending.Prints_ == baked.Prints.size() &&
         pending.Spread_ == baked.SeatSpreadM.size() && pending.Across_ == baked.AcrossM.size());
  const auto nextTile = std::ranges::lower_bound(AcceptedTiles_, pending.Tile_);
  const uint32_t firstPrint = nextTile == AcceptedTiles_.end()
                                  ? static_cast<uint32_t>(Prints_.size())
                                  : ByTile_.At(*nextTile).First;
  Prints_.insert(Prints_.begin() + firstPrint, baked.Prints.begin(), baked.Prints.end());
  SeatSpread_.insert(SeatSpread_.end(), baked.SeatSpreadM.begin(), baked.SeatSpreadM.end());
  Across_.insert(Across_.end(), baked.AcrossM.begin(), baked.AcrossM.end());
  OsmHeights_ += baked.OsmHeights;
  DefaultHeights_ += baked.DefaultHeights;
  Fronted_ += baked.Fronted;
  TrianglesHanded_ += baked.Triangles;
  ByTile_.ShiftAfter(
      {.AfterTile = pending.Tile_, .By = static_cast<uint32_t>(baked.Prints.size())});
  ByTile_.Set(pending.Tile_, firstPrint, firstPrint + static_cast<uint32_t>(baked.Prints.size()));
  AcceptedTiles_.insert(nextTile, pending.Tile_);
  Mark_.Advance(field.Features());
  ++Accepted_;
  ++Revision_;
}

bool BuildingField::IngestedWithin(const OsmField &field, int rings) const noexcept {
  if (rings < 0) { return false; }
  const std::span<const OsmField::Feature> features = field.Features();
  size_t at = 0;
  while (at < features.size()) {
    const uint32_t tile = features[at].Tile;
    while (at < features.size() && features[at].Tile == tile) { ++at; }
    if (tile >= field.Tiles().size()) { return false; }
    const OsmField::Tile &source = field.Tiles()[tile];
    if (std::abs(source.X - field.CentreX()) > rings ||
        std::abs(source.Y - field.CentreY()) > rings) {
      continue;
    }
    if (!std::ranges::binary_search(AcceptedTiles_, tile)) { return false; }
  }
  return true;
}

}
