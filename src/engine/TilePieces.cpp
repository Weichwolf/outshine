#include "TilePieces.h"
#include "math/RenderFrame.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <expected>
#include <optional>
#include <span>
#include <string>

#include "Digest.h"
#include "SceneRenderer.h"
#include "Shape.h"

namespace outshine {

namespace {

std::expected<Render::PieceHandle, std::string>
PlaceStructurePiece(Render::SceneRenderer &renderer,
                    std::span<const StoredVertex> corners,
                    std::span<const uint32_t> run,
                    const ClusteredMesh &cut,
                    Render::PieceSurface surface,
                    const Mat4 &row,
                    bool textured) {
  if (run.empty()) { return Render::PieceHandle{}; }
  if (textured && (std::ranges::any_of(corners,
                                       [](const StoredVertex &v) {
                                         return !std::isfinite(v.uv()[0]) ||
                                                !std::isfinite(v.uv()[1]);
                                       }) ||
                   std::ranges::all_of(corners, [](const StoredVertex &v) {
                     return v.uv()[0] == 0.0f && v.uv()[1] == 0.0f;
                   }))) {
    return std::unexpected("generated building wall has invalid facade coordinates");
  }
  const bool cooked = cut.Index.size() == run.size() && !cut.Clusters.empty();
  return renderer.PlaceStructurePiece(
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
}

}

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

bool TilePieces::ShouldShow(uint32_t tile, std::optional<LevelOfDetail> detail) const {
  if (!detail) { return true; }
  const auto first = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto previous = std::find_if(first, Standing_.end(), [tile, detail](const Standing &held) {
    return held.Tile == tile && held.Detail == detail;
  });
  if (previous != Standing_.end()) { return previous->Visible; }
  return std::none_of(first, Standing_.end(), [tile](const Standing &held) {
    return held.Tile == tile && held.Visible;
  });
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
  const bool visible = ShouldShow(tile, baked.RequestedDetail);
  Standing stood{.Tile = tile,
                 .Digest = baked.Digest,
                 .FallbackHeights = baked.FallbackHeights,
                 .Detail = baked.RequestedDetail,
                 .Row = row,
                 .Visible = visible};
  const Raised &built = baked.Built;
  const auto walls = PlaceStructurePiece(
      *Renderer_, built.WallCorners, built.WallRun, baked.Walls, WallsSurface_, row, true);
  if (!walls) {
    ++Refused_;
    Why_ = walls.error();
    error = Why_;
    return false;
  }
  stood.Walls = *walls;
  const auto roofs = PlaceStructurePiece(
      *Renderer_, built.RoofCorners, built.RoofRun, baked.Roofs, RoofsSurface_, row, false);
  if (!roofs) {
    Releases(stood);
    ++Refused_;
    Why_ = roofs.error();
    error = Why_;
    return false;
  }
  stood.Roofs = *roofs;
  if (!visible) {
    std::array<Render::SceneRenderer::PieceRows, 2> rows;
    size_t count = 0;
    if (stood.Walls) { rows[count++] = {.Piece = stood.Walls, .Rows = {}}; }
    if (stood.Roofs) { rows[count++] = {.Piece = stood.Roofs, .Rows = {}}; }
    if (!Renderer_->SetPieceInstances(std::span(rows.data(), count), error)) {
      Releases(stood);
      ++Refused_;
      Why_ = error;
      return false;
    }
  }
  if (baked.RequestedDetail) {
    ForgetsDetail(tile, baked.RequestedDetail);
  } else {
    Forgets(tile);
  }
  if (stood.Walls || stood.Roofs) {
    Standing_.insert(std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile), stood);
  }
  RefreshDigest();
  ++Handed_;
  return true;
}

void TilePieces::Forgets(uint32_t tile) {
  auto at = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  while (at != Standing_.end() && at->Tile == tile) {
    Releases(*at);
    at = Standing_.erase(at);
  }
  RefreshDigest();
}

void TilePieces::ForgetsDetail(uint32_t tile, std::optional<LevelOfDetail> detail) {
  const auto first = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto at = std::find_if(first, Standing_.end(), [tile, detail](const Standing &held) {
    return held.Tile == tile && held.Detail == detail;
  });
  if (at == Standing_.end()) { return; }
  Releases(*at);
  Standing_.erase(at);
  RefreshDigest();
}

void TilePieces::Releases(const Standing &stood) {
  if (Renderer_ == nullptr) { return; }
  if (stood.Walls) { Renderer_->ReleasePiece(stood.Walls); }
  if (stood.Roofs) { Renderer_->ReleasePiece(stood.Roofs); }
}

bool TilePieces::SelectDetail(uint32_t tile, LevelOfDetail detail, std::string &error) {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  const auto first = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto target = std::find_if(first, Standing_.end(), [tile, detail](const Standing &held) {
    return held.Tile == tile && held.Detail == detail;
  });
  if (target == Standing_.end()) {
    error = "the requested structure detail is not resident";
    return false;
  }
  if (target->Visible) { return true; }
  const auto current = std::find_if(first, Standing_.end(), [tile](const Standing &held) {
    return held.Tile == tile && held.Visible;
  });
  std::array<Render::SceneRenderer::PieceRows, 4> rows;
  size_t count = 0;
  if (current != Standing_.end()) {
    if (current->Walls) { rows[count++] = {.Piece = current->Walls, .Rows = {}}; }
    if (current->Roofs) { rows[count++] = {.Piece = current->Roofs, .Rows = {}}; }
  }
  if (target->Walls) { rows[count++] = {.Piece = target->Walls, .Rows = {&target->Row, 1}}; }
  if (target->Roofs) { rows[count++] = {.Piece = target->Roofs, .Rows = {&target->Row, 1}}; }
  if (!Renderer_->SetPieceInstances(std::span(rows.data(), count), error)) { return false; }
  if (current != Standing_.end()) { current->Visible = false; }
  target->Visible = true;
  RefreshDigest();
  return true;
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
    if (!stood.Visible) { continue; }
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
