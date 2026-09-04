#include "BuildingField.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace outshine::Ground {

void BuildingField::AnchorAt(const Vec3 &ecef) {
  assert(Prints_.empty());
  for (int c = 0; c < 3; c++) { Anchor_[c] = ecef[c]; }
  Anchored_ = true;
}

double BuildingField::AwayFromCentreM(const OsmField &field, uint32_t tile) const {
  const std::span<const OsmField::Tile> tiles = field.Tiles();
  if (tile >= tiles.size() || !(TileSpanM_ > 0.0)) { return 0.0; }
  const auto across = static_cast<double>(tiles[tile].X - field.CentreX());
  const auto down = static_cast<double>(tiles[tile].Y - field.CentreY());
  return std::sqrt(across * across + down * down) * TileSpanM_;
}

std::optional<TileWatermark::Next>
BuildingField::Next(const OsmField &field, const std::function<bool(FeatureRun)> &groundStands) {
  assert(Anchored_);
  const std::span<const OsmField::Feature> feats = field.Features();
  if (Mark_.Done(feats)) { return std::nullopt; }
  const TileWatermark::Next next = Mark_.Ask(
      feats,
      field.Tiles(),
      {.CentreX = field.CentreX(), .CentreY = field.CentreY(), .Rings = kEveryRing},
      [&groundStands](size_t from, size_t to) { return groundStands({.From = from, .To = to}); });
  if (!next.Found) { return std::nullopt; }
  return next;
}

void BuildingField::Accept(uint32_t tile, const OsmField &field, const Baked &baked) {
  const auto firstPrint = static_cast<uint32_t>(Prints_.size());
  Prints_.insert(Prints_.end(), baked.Prints.begin(), baked.Prints.end());
  SeatSpread_.insert(SeatSpread_.end(), baked.SeatSpreadM.begin(), baked.SeatSpreadM.end());
  Across_.insert(Across_.end(), baked.AcrossM.begin(), baked.AcrossM.end());
  OsmHeights_ += baked.OsmHeights;
  DefaultHeights_ += baked.DefaultHeights;
  Fronted_ += baked.Fronted;
  TrianglesHanded_ += baked.Triangles;
  ByTile_.Set(tile, firstPrint, static_cast<uint32_t>(Prints_.size()));
  Mark_.Advance(field.Features());
  ++Accepted_;
}

} // namespace outshine::Ground
