#include "BuildingField.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace outshine::Ground {

static_assert(std::is_nothrow_copy_constructible_v<BuildingField::Footprint>);
static_assert(std::is_nothrow_move_assignable_v<BuildingField::Footprint>);
static_assert(std::is_nothrow_move_constructible_v<BuildingField::AcceptedInput>);
static_assert(std::is_nothrow_move_assignable_v<BuildingField::AcceptedInput>);

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
  AcceptedInputs_.clear();
  Products_.clear();
  TrianglesHanded_ = 0;
  Taken_ = Accepted_ = 0;
  RefinementAt_ = RefinementEnd_ = 0;
  RefinementActive_ = false;
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
  AcceptedInputs_.reserve(AcceptedInputs_.size() + capacity.Tiles);
  Products_.reserve(Products_.size() + capacity.Tiles);
}

BuildingField::PendingAcceptance
BuildingField::PrepareAcceptance(uint32_t tile,
                                 const Baked &baked,
                                 std::span<const Data::TileSourceIdentity> sources,
                                 bool qualified,
                                 std::optional<Data::TileSourceIdentity> vector) {
  return PrepareAcceptance(tile, baked, sources, qualified, std::move(vector), BakeInputs{});
}

BuildingField::PendingAcceptance
BuildingField::PrepareAcceptance(uint32_t tile,
                                 const Baked &baked,
                                 std::span<const Data::TileSourceIdentity> sources,
                                 bool qualified,
                                 std::optional<Data::TileSourceIdentity> vector,
                                 BakeInputs bake) {
  return {this, tile, baked, std::move(vector), sources, qualified, bake};
}

void BuildingField::CommitAcceptance(PendingAcceptance pending,
                                     const OsmField &field,
                                     const Baked &baked) noexcept {
  assert(pending.Owner_ == this && pending.Prints_ == baked.Prints.size() &&
         pending.Spread_ == baked.SeatSpreadM.size() && pending.Across_ == baked.AcrossM.size());
  assert(Prints_.capacity() - Prints_.size() >= baked.Prints.size() &&
         SeatSpread_.capacity() - SeatSpread_.size() >= baked.SeatSpreadM.size() &&
         Across_.capacity() - Across_.size() >= baked.AcrossM.size() &&
         AcceptedTiles_.size() < AcceptedTiles_.capacity() &&
         AcceptedInputs_.size() < AcceptedInputs_.capacity() &&
         Products_.size() < Products_.capacity());
  const auto nextTile = std::ranges::lower_bound(AcceptedTiles_, pending.Tile_);
  assert(nextTile == AcceptedTiles_.end() || *nextTile != pending.Tile_);
  const size_t at = static_cast<size_t>(nextTile - AcceptedTiles_.begin());
  const size_t firstPrint = at == Products_.size() ? Prints_.size() : Products_[at].Prints.First;
  const size_t firstSpread =
      at == Products_.size() ? SeatSpread_.size() : Products_[at].Spread.First;
  const size_t firstAcross = at == Products_.size() ? Across_.size() : Products_[at].Across.First;
  Prints_.insert(Prints_.begin() + static_cast<ptrdiff_t>(firstPrint),
                 baked.Prints.begin(),
                 baked.Prints.end());
  SeatSpread_.insert(SeatSpread_.begin() + static_cast<ptrdiff_t>(firstSpread),
                     baked.SeatSpreadM.begin(),
                     baked.SeatSpreadM.end());
  Across_.insert(Across_.begin() + static_cast<ptrdiff_t>(firstAcross),
                 baked.AcrossM.begin(),
                 baked.AcrossM.end());
  for (size_t later = at; later < Products_.size(); ++later) {
    Products_[later].Prints.First += baked.Prints.size();
    Products_[later].Spread.First += baked.SeatSpreadM.size();
    Products_[later].Across.First += baked.AcrossM.size();
  }
  OsmHeights_ += baked.OsmHeights;
  DefaultHeights_ += baked.DefaultHeights;
  Fronted_ += baked.Fronted;
  TrianglesHanded_ += baked.Triangles;
  Products_.insert(Products_.begin() + static_cast<ptrdiff_t>(at),
                   {.Prints = {.First = firstPrint, .Count = baked.Prints.size()},
                    .Spread = {.First = firstSpread, .Count = baked.SeatSpreadM.size()},
                    .Across = {.First = firstAcross, .Count = baked.AcrossM.size()},
                    .Triangles = baked.Triangles,
                    .OsmHeights = baked.OsmHeights,
                    .DefaultHeights = baked.DefaultHeights,
                    .Fronted = baked.Fronted});
  AcceptedInputs_.insert(AcceptedInputs_.begin() + static_cast<ptrdiff_t>(at),
                         std::move(pending.Input_));
  AcceptedTiles_.insert(nextTile, pending.Tile_);
  Mark_.Advance(field.Features());
  ++Accepted_;
  ++Revision_;
}

void BuildingField::ReplaceAcceptance(PendingAcceptance pending, const Baked &baked) noexcept {
  assert(pending.Owner_ == this && pending.Prints_ == baked.Prints.size() &&
         pending.Spread_ == baked.SeatSpreadM.size() && pending.Across_ == baked.AcrossM.size());
  const auto tile = std::ranges::lower_bound(AcceptedTiles_, pending.Tile_);
  assert(tile != AcceptedTiles_.end() && *tile == pending.Tile_);
  const size_t at = static_cast<size_t>(tile - AcceptedTiles_.begin());
  const TileProduct old = Products_[at];
  assert(Prints_.capacity() - Prints_.size() + old.Prints.Count >= baked.Prints.size() &&
         SeatSpread_.capacity() - SeatSpread_.size() + old.Spread.Count >=
             baked.SeatSpreadM.size() &&
         Across_.capacity() - Across_.size() + old.Across.Count >= baked.AcrossM.size());
  const auto replace = [](auto &values, Range range, auto incoming) {
    auto first = values.begin() + static_cast<ptrdiff_t>(range.First);
    first = values.erase(first, first + static_cast<ptrdiff_t>(range.Count));
    values.insert(first, incoming.begin(), incoming.end());
  };
  replace(Prints_, old.Prints, baked.Prints);
  replace(SeatSpread_, old.Spread, baked.SeatSpreadM);
  replace(Across_, old.Across, baked.AcrossM);
  const auto shifted = [](size_t first, size_t oldCount, size_t newCount) {
    return newCount >= oldCount ? first + newCount - oldCount : first - (oldCount - newCount);
  };
  for (size_t later = at + 1; later < Products_.size(); ++later) {
    Products_[later].Prints.First =
        shifted(Products_[later].Prints.First, old.Prints.Count, baked.Prints.size());
    Products_[later].Spread.First =
        shifted(Products_[later].Spread.First, old.Spread.Count, baked.SeatSpreadM.size());
    Products_[later].Across.First =
        shifted(Products_[later].Across.First, old.Across.Count, baked.AcrossM.size());
  }
  Products_[at] = {.Prints = {.First = old.Prints.First, .Count = baked.Prints.size()},
                   .Spread = {.First = old.Spread.First, .Count = baked.SeatSpreadM.size()},
                   .Across = {.First = old.Across.First, .Count = baked.AcrossM.size()},
                   .Triangles = baked.Triangles,
                   .OsmHeights = baked.OsmHeights,
                   .DefaultHeights = baked.DefaultHeights,
                   .Fronted = baked.Fronted};
  AcceptedInputs_[at] = std::move(pending.Input_);
  TrianglesHanded_ = TrianglesHanded_ - old.Triangles + baked.Triangles;
  OsmHeights_ += baked.OsmHeights - old.OsmHeights;
  DefaultHeights_ += baked.DefaultHeights - old.DefaultHeights;
  Fronted_ += baked.Fronted - old.Fronted;
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
