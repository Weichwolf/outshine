#include "TilePieces.h"
#include "math/RenderFrame.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "Digest.h"
#include "SceneRenderer.h"
#include "Shape.h"

namespace outshine {

Mat4 TilePieces::RowFor(const Vec3 &anchorEcef) const {
  const Vec3 &east = Frame_.EastEcef();
  const Vec3 &north = Frame_.NorthEcef();
  const Vec3 &up = Frame_.UpEcef();
  const EastNorthUp shift = Frame_.ToLocalPosition(anchorEcef);
  Mat4 row;
  for (size_t axis = 0; axis < 3; ++axis) {
    row[axis * 4u] = east[axis];
    row[axis * 4u + 1u] = up[axis];
    row[axis * 4u + 2u] = RenderFrame::ZOfNorth(north[axis]);
    row[axis * 4u + 3u] = 0.0;
  }
  row[12] = shift.EastM;
  row[13] = shift.UpM;
  row[14] = RenderFrame::ZOfNorth(shift.NorthM);
  row[15] = 1.0;
  return row;
}

bool TilePieces::Hands(uint32_t tile,
                       const Generators::BakedTile &baked,
                       const Vec3 &anchorEcef,
                       std::string &error) {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  const Mat4 row = RowFor(anchorEcef);
  Standing stood{.Tile = tile, .Digest = baked.Digest, .FallbackHeights = baked.FallbackHeights};
  std::string why;
  const auto place = [this, &row, &why](std::span<const StoredVertex> corners,
                                        std::span<const uint32_t> run,
                                        const ClusteredMesh &cut,
                                        Render::PieceSurface surface,
                                        bool textured) {
    if (run.empty()) { return Render::PieceHandle{}; }
    if (textured && std::ranges::any_of(corners, [](const StoredVertex &v) {
          return v.uv()[0] == 0.0f && v.uv()[1] == 0.0f;
        })) {
      why = "generated building wall is missing facade coordinates";
      return Render::PieceHandle{};
    }
    const bool cooked = cut.Index.size() == run.size() && !cut.Clusters.empty();
    const auto placed = Renderer_->PlaceStructurePiece(
        {.Tangents = {},
         .Verts = corners,
         .Indices = cooked ? std::span<const uint32_t>(cut.Index) : run,
         .Clusters =
             cooked ? std::span<const DagCluster>(cut.Clusters) : std::span<const DagCluster>(),
         .Colours = {},
         .Row = row,
         .Instances = {},
         .Surface = surface,
         .Textured = textured});
    if (!placed) {
      why = placed.error();
      return Render::PieceHandle{};
    }
    return *placed;
  };
  const Raised &built = baked.Built;
  stood.Walls = place(built.WallCorners, built.WallRun, baked.Walls, WallsSurface_, true);
  if (!why.empty()) {
    ++Refused_;
    Why_ = why;
    error = why;
    return false;
  }
  stood.Roofs = place(built.RoofCorners, built.RoofRun, baked.Roofs, RoofsSurface_, false);
  if (!why.empty()) {
    if (stood.Walls) { Renderer_->ReleasePiece(stood.Walls); }
    ++Refused_;
    Why_ = why;
    error = why;
    return false;
  }
  Forgets(tile);
  if (stood.Walls || stood.Roofs) {
    Standing_.insert(std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile), stood);
  }
  RefreshDigest();
  ++Handed_;
  return true;
}

void TilePieces::Forgets(uint32_t tile) {
  const auto at = std::ranges::find(Standing_, tile, &Standing::Tile);
  if (at == Standing_.end()) { return; }
  if (Renderer_ != nullptr) {
    if (at->Walls) { Renderer_->ReleasePiece(at->Walls); }
    if (at->Roofs) { Renderer_->ReleasePiece(at->Roofs); }
  }
  Standing_.erase(at);
  RefreshDigest();
}

bool TilePieces::ValidateSources(std::string &error) const {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  for (const Standing &stood : Standing_) {
    for (const Render::PieceHandle piece : {stood.Walls, stood.Roofs}) {
      if (!piece || Renderer_->HasPieceSource(piece)) { continue; }
      error = "structure tile " + std::to_string(stood.Tile) + " holds missing piece slot " +
              std::to_string(piece.Slot) + ":" + std::to_string(piece.Generation);
      return false;
    }
  }
  return true;
}

void TilePieces::RefreshDigest() noexcept {
  if (Standing_.empty()) {
    Digest_ = 0;
    return;
  }
  Digest_ = kDigestBasis;
  for (const Standing &stood : Standing_) {
    Digest_ = (Digest_ ^ static_cast<uint64_t>(stood.Tile)) * kDigestPrime;
    Digest_ = (Digest_ ^ stood.Digest) * kDigestPrime;
  }
}

void TilePieces::Clear() {
  while (!Standing_.empty()) { Forgets(Standing_.back().Tile); }
  Renderer_ = nullptr;
  Digest_ = 0;
  Handed_ = 0;
  Refused_ = 0;
  Why_.clear();
}

}
