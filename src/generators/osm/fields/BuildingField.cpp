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

namespace outshine::Generators::Osm {

using namespace outshine::Ground;

static_assert(std::is_nothrow_copy_constructible_v<BuildingFootprint>);
static_assert(std::is_nothrow_move_assignable_v<BuildingFootprint>);
static_assert(std::is_nothrow_move_constructible_v<BuildingField::AcceptedInput>);
static_assert(std::is_nothrow_move_assignable_v<BuildingField::AcceptedInput>);

BuildingField BuildingField::SnapshotAccepted() const {
  BuildingField copy = *this;
  const size_t released = copy.Admission_.ReleaseUnaccepted(copy.AcceptedTiles_);
  copy.Taken_ -= released;
  assert(copy.Taken_ == copy.Accepted_);
  return copy;
}

void BuildingField::ResetDerived() {
  ReservationOwner_ = ReservationDomain{};
  ++Revision_;
  ContactBytes_ = 0;
  AcceptedTiles_.clear();
  AcceptedInputs_.clear();
  Products_.clear();
  TrianglesHanded_ = 0;
  Taken_ = Accepted_ = 0;
  RefinementAt_ = RefinementEnd_ = 0;
  RefinementActive_ = false;
  Admission_ = {};
  OsmHeights_ = DefaultHeights_ = Fronted_ = 0;
}

void BuildingField::AnchorAt(const Vec3 &ecef) {
  assert(FootprintCount() == 0);
  for (int c = 0; c < 3; c++) { Anchor_[c] = ecef[c]; }
  Anchored_ = true;
}

std::optional<TileAdmission::Next>
BuildingField::Next(const OsmField &field,
                    const std::function<bool(FeatureRun)> &groundStands,
                    size_t candidatesMost) {
  assert(Anchored_);
  if (Admission_.Done(field.Tiles())) { return std::nullopt; }
  const TileAdmission::Next next = Admission_.Ask(
      field.Tiles(),
      {.CentreX = field.CentreX(),
       .CentreY = field.CentreY(),
       .Rings = kEveryRing,
       .CandidatesMost = candidatesMost},
      [&groundStands](size_t from, size_t to) { return groundStands({.From = from, .To = to}); });
  if (!next.Found) { return std::nullopt; }
  return next;
}

void BuildingField::PreparesAcceptances(size_t tiles) {
  AcceptedTiles_.reserve(AcceptedTiles_.size() + tiles);
  AcceptedInputs_.reserve(AcceptedInputs_.size() + tiles);
  Products_.reserve(Products_.size() + tiles);
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
                                 BakeInputs bake,
                                 uint64_t heightRevision,
                                 HeightField::Request heights) {
  PreparesAcceptances(std::ranges::binary_search(AcceptedTiles_, tile) ? 0 : 1);
  return {this,
          tile,
          baked,
          std::move(vector),
          sources,
          qualified,
          std::move(bake),
          heightRevision,
          std::move(heights)};
}

void BuildingField::CommitAcceptance(PendingAcceptance pending, const Baked &baked) noexcept {
  assert(pending.Owner_ == this && pending.Prints_ == baked.Prints.size() &&
         pending.Spread_ == baked.SeatSpreadM.size() && pending.Across_ == baked.AcrossM.size());
  assert(AcceptedTiles_.size() < AcceptedTiles_.capacity() &&
         AcceptedInputs_.size() < AcceptedInputs_.capacity() &&
         Products_.size() < Products_.capacity());
  const auto nextTile = std::ranges::lower_bound(AcceptedTiles_, pending.Tile_);
  assert(nextTile == AcceptedTiles_.end() || *nextTile != pending.Tile_);
  const size_t at = static_cast<size_t>(nextTile - AcceptedTiles_.begin());
  const size_t firstPrint = at == Products_.size() ? FootprintCount() : Products_[at].Prints.First;
  const size_t contactBytes = pending.Physical_->ContactBytes;
  for (size_t later = at; later < Products_.size(); ++later) {
    Products_[later].Prints.First += pending.Prints_;
  }
  Products_.insert(Products_.begin() + static_cast<ptrdiff_t>(at),
                   {.Prints = {.First = firstPrint, .Count = pending.Prints_},
                    .Physical = std::move(pending.Physical_),
                    .Triangles = baked.Triangles,
                    .OsmHeights = baked.OsmHeights,
                    .DefaultHeights = baked.DefaultHeights,
                    .Fronted = baked.Fronted});
  AcceptedInputs_.insert(AcceptedInputs_.begin() + static_cast<ptrdiff_t>(at),
                         std::move(pending.Input_));
  AcceptedTiles_.insert(nextTile, pending.Tile_);
  OsmHeights_ += baked.OsmHeights;
  DefaultHeights_ += baked.DefaultHeights;
  Fronted_ += baked.Fronted;
  TrianglesHanded_ += baked.Triangles;
  ContactBytes_ += contactBytes;
  ++Accepted_;
  ++Revision_;
}

void BuildingField::ReplaceAcceptance(PendingAcceptance pending, const Baked &baked) noexcept {
  assert(pending.Owner_ == this && pending.Prints_ == baked.Prints.size() &&
         pending.Spread_ == baked.SeatSpreadM.size() && pending.Across_ == baked.AcrossM.size());
  const auto tile = std::ranges::lower_bound(AcceptedTiles_, pending.Tile_);
  assert(tile != AcceptedTiles_.end() && *tile == pending.Tile_);
  const size_t at = static_cast<size_t>(tile - AcceptedTiles_.begin());
  const TileProduct &old = Products_[at];
  const AcceptedInput &before = AcceptedInputs_[at];
  const AcceptedInput &after = pending.Input_;
  const bool semanticChanged =
      old.Physical != pending.Physical_ || before.Vector != after.Vector ||
      before.Sources != after.Sources || before.OccupiedCells != after.OccupiedCells ||
      before.CellBounds != after.CellBounds || before.CellMaxHeightM != after.CellMaxHeightM ||
      before.Qualified != after.Qualified || before.SourceKey != after.SourceKey ||
      before.Bake.HeightRasterDigest != after.Bake.HeightRasterDigest ||
      before.Bake.StreetDigest != after.Bake.StreetDigest ||
      before.Bake.TileSpanM != after.Bake.TileSpanM;
  const size_t newCount = pending.Prints_;
  const size_t oldCount = old.Prints.Count;
  for (size_t later = at + 1; later < Products_.size(); ++later) {
    auto &first = Products_[later].Prints.First;
    first = newCount >= oldCount ? first + newCount - oldCount : first - (oldCount - newCount);
  }
  TrianglesHanded_ = TrianglesHanded_ - old.Triangles + baked.Triangles;
  ContactBytes_ = ContactBytes_ - old.Physical->ContactBytes + pending.Physical_->ContactBytes;
  OsmHeights_ += baked.OsmHeights - old.OsmHeights;
  DefaultHeights_ += baked.DefaultHeights - old.DefaultHeights;
  Fronted_ += baked.Fronted - old.Fronted;
  Products_[at] = {.Prints = {.First = old.Prints.First, .Count = newCount},
                   .Physical = std::move(pending.Physical_),
                   .Triangles = baked.Triangles,
                   .OsmHeights = baked.OsmHeights,
                   .DefaultHeights = baked.DefaultHeights,
                   .Fronted = baked.Fronted};
  AcceptedInputs_[at] = std::move(pending.Input_);
  if (semanticChanged) { ++Revision_; }
}

bool BuildingField::IngestedWithin(const OsmField &field, int rings) const noexcept {
  if (rings < 0) { return false; }
  for (size_t tile = 0; tile < field.Tiles().size(); ++tile) {
    const auto &source = field.Tiles()[tile];
    if (source.FeatureCount == 0 || std::abs(source.X - field.CentreX()) > rings ||
        std::abs(source.Y - field.CentreY()) > rings) {
      continue;
    }
    if (!std::ranges::binary_search(AcceptedTiles_, static_cast<uint32_t>(tile))) { return false; }
  }
  return true;
}

}
