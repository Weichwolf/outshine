#include "TilePieces.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "Digest.h"
#include "Live.h"
#include "Shape.h"

namespace outshine {

Mat4 TilePieces::RowFor(const Vec3 &anchorEcef) const {
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  const Vec3 &up = Frame_.UpEcef();
  const EastNorthUp shift = Frame_.Place(anchorEcef);
  Mat4 row;
  for (size_t axis = 0; axis < 3; ++axis) {
    row[axis * 4u] = east[axis];
    row[axis * 4u + 1u] = up[axis];
    row[axis * 4u + 2u] = -north[axis];
    row[axis * 4u + 3u] = 0.0;
  }
  row[12] = shift.EastM;
  row[13] = shift.UpM;
  row[14] = -shift.NorthM;
  row[15] = 1.0;
  return row;
}

void TilePieces::Hands(uint32_t tile, const Generators::BakedTile &baked, const Vec3 &anchorEcef) {
  Forgets(tile);
  if (Live_ == nullptr) { return; }
  const Mat4 row = RowFor(anchorEcef);
  Standing stood{.Tile = tile};
  std::string why;
  const auto place = [this, &row, &why](std::span<const StoredVertex> corners,
                                        std::span<const uint32_t> run,
                                        const Cooked &cut,
                                        uint32_t surface) {
    if (run.size() < 3) { return Render::kNoPiece; }
    const bool cooked = cut.Index.size() == run.size() && !cut.Clusters.empty();
    return Live_->PlacePiece({.Verts = corners,
                              .Indices = cooked ? std::span<const uint32_t>(cut.Index) : run,
                              .Clusters = cooked ? std::span<const DagCluster>(cut.Clusters)
                                                 : std::span<const DagCluster>(),
                              .Row = row,
                              .Surface = surface},
                             why);
  };
  const Raised &built = baked.Built;
  stood.Walls = place(built.WallCorners, built.WallRun, baked.Walls, WallsSurface_);
  stood.Roofs = place(built.RoofCorners, built.RoofRun, baked.Roofs, RoofsSurface_);
  if (!why.empty()) {
    ++Refused_;
    Why_ = why;
  }
  Standing_.push_back(stood);
  Digest_ = (Digest_ ^ baked.Digest) * kDigestPrime;
  ++Handed_;
}

void TilePieces::Forgets(uint32_t tile) {
  const auto at = std::ranges::find(Standing_, tile, &Standing::Tile);
  if (at == Standing_.end()) { return; }
  if (Live_ != nullptr) {
    if (at->Walls != Render::kNoPiece) { Live_->ReleasePiece(at->Walls); }
    if (at->Roofs != Render::kNoPiece) { Live_->ReleasePiece(at->Roofs); }
  }
  Standing_.erase(at);
}

void TilePieces::Clear() {
  while (!Standing_.empty()) { Forgets(Standing_.back().Tile); }
  Digest_ = 0;
  Handed_ = 0;
  Refused_ = 0;
  Why_.clear();
}

} // namespace outshine
