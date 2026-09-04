#include "TilePieces.h"

#include <algorithm>
#include <bit>

#include "Digest.h"
#include "Live.h"
#include "Shape.h"
#include "spatial/ClusterCook.h"

namespace outshine {

namespace {

[[nodiscard]] uint64_t DigestOver(const Raised &built) {
  uint64_t mixed = kDigestBasis;
  const auto fold = [&mixed](uint64_t one) { mixed = (mixed ^ one) * kDigestPrime; };
  const auto foldVertex = [&fold](const StoredVertex &one) {
    const auto *const held = reinterpret_cast<const float *>(&one);
    for (size_t at = 0; at < kStoredVertexFloats; ++at) { fold(std::bit_cast<uint32_t>(held[at])); }
  };
  for (const StoredVertex &one : built.WallCorners) { foldVertex(one); }
  for (const StoredVertex &one : built.RoofCorners) { foldVertex(one); }
  for (const uint32_t one : built.WallRun) { fold(one); }
  for (const uint32_t one : built.RoofRun) { fold(one); }
  return mixed;
}

} // namespace

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

void TilePieces::Hands(uint32_t tile, const Raised &built, const Vec3 &anchorEcef) {
  Forgets(tile);
  if (Live_ == nullptr) { return; }
  const Mat4 row = RowFor(anchorEcef);
  Standing stood{.Tile = tile};
  std::string why;
  const auto place = [this, &row, &why](std::span<const StoredVertex> corners,
                                        std::span<const uint32_t> run,
                                        uint32_t surface) {
    if (run.size() < 3) { return Render::kNoPiece; }
    const std::span<const float> positions(reinterpret_cast<const float *>(corners.data()),
                                           corners.size() * kStoredVertexFloats);
    const Cooked cut = CookClusters(
        positions, run, Render::kClusterTriangles, static_cast<int>(kStoredVertexFloats));
    const bool cooked = cut.Index.size() == run.size() && !cut.Clusters.empty();
    return Live_->PlacePiece({.Verts = corners,
                              .Indices = cooked ? std::span<const uint32_t>(cut.Index) : run,
                              .Clusters = cooked ? std::span<const DagCluster>(cut.Clusters)
                                                 : std::span<const DagCluster>(),
                              .Row = row,
                              .Surface = surface},
                             why);
  };
  stood.Walls = place(built.WallCorners, built.WallRun, WallsSurface_);
  stood.Roofs = place(built.RoofCorners, built.RoofRun, RoofsSurface_);
  if (!why.empty()) {
    ++Refused_;
    Why_ = why;
  }
  Standing_.push_back(stood);
  Digest_ = (Digest_ ^ DigestOver(built)) * kDigestPrime;
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
