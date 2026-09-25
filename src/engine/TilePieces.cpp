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
#include <utility>
#include <vector>

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

void AppendPieceRows(std::vector<Render::SceneRenderer::PieceRows> &rows,
                     Render::PieceHandle walls,
                     Render::PieceHandle roofs,
                     const Mat4 *row) {
  const std::span<const Mat4> instances =
      row == nullptr ? std::span<const Mat4>{} : std::span<const Mat4>{row, 1};
  if (walls) { rows.push_back({.Piece = walls, .Rows = instances}); }
  if (roofs) { rows.push_back({.Piece = roofs, .Rows = instances}); }
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

bool TilePieces::ShouldShow(uint32_t tile,
                            uint32_t cell,
                            std::optional<LevelOfDetail> detail,
                            uint64_t sourceKey) const {
  if (!detail) { return true; }
  const auto first = std::ranges::lower_bound(Standing_, std::pair{tile, cell}, {}, AddressOf);
  const auto last = std::find_if(first, Standing_.end(), [tile, cell](const Standing &held) {
    return held.Tile != tile || held.Cell != cell;
  });
  if (std::any_of(
          first, last, [sourceKey](const Standing &held) { return held.SourceKey != sourceKey; })) {
    return true;
  }
  const auto previous =
      std::find_if(first, last, [detail](const Standing &held) { return held.Detail == detail; });
  if (previous != last) { return previous->Visible; }
  return std::none_of(first, last, [](const Standing &held) { return held.Visible; });
}

bool TilePieces::Hands(uint32_t tile,
                       const Generators::BakedTile &baked,
                       const Vec3 &anchorEcef,
                       std::string &error,
                       uint64_t sourceKey) {
  return Hands(tile, 0, baked, anchorEcef, error, sourceKey);
}

bool TilePieces::Hands(uint32_t tile,
                       uint32_t cell,
                       const Generators::BakedTile &baked,
                       const Vec3 &anchorEcef,
                       std::string &error,
                       uint64_t sourceKey) {
  return Store(tile, cell, baked, anchorEcef, error, sourceKey, false);
}

bool TilePieces::StageCell(uint32_t tile,
                           uint32_t cell,
                           const Generators::BakedTile &baked,
                           const Vec3 &anchorEcef,
                           std::string &error,
                           uint64_t sourceKey) {
  return Store(tile, cell, baked, anchorEcef, error, sourceKey, true);
}

bool TilePieces::Store(uint32_t tile,
                       uint32_t cell,
                       const Generators::BakedTile &baked,
                       const Vec3 &anchorEcef,
                       std::string &error,
                       uint64_t sourceKey,
                       bool staged) {
  if (!ValidateStore({.Tile = tile, .Cell = cell}, baked, sourceKey, staged, error)) {
    return false;
  }
  const Mat4 row = RowFor(anchorEcef);
  const auto first = std::ranges::lower_bound(Standing_, std::pair{tile, cell}, {}, AddressOf);
  const auto last = std::find_if(first, Standing_.end(), [tile, cell](const Standing &held) {
    return held.Tile != tile || held.Cell != cell;
  });
  const bool sourceChanged = std::any_of(
      first, last, [sourceKey](const Standing &held) { return held.SourceKey != sourceKey; });
  const bool visible = !staged && ShouldShow(tile, cell, baked.RequestedDetail, sourceKey);
  Standing stood{.Tile = tile,
                 .Cell = cell,
                 .Digest = baked.Digest,
                 .SourceKey = sourceKey,
                 .OccupiedCells = baked.OccupiedCells,
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
  if (staged) { DiscardSupersededStage(tile, sourceKey); }
  if (!staged) {
    if (baked.RequestedDetail && !sourceChanged) {
      ForgetsDetail(tile, cell, baked.RequestedDetail);
    } else {
      ForgetsCell(tile, cell);
    }
  }
  if (stood.Walls || stood.Roofs || staged) {
    Standing_.insert(std::ranges::lower_bound(Standing_, std::pair{tile, cell}, {}, AddressOf),
                     stood);
  }
  RefreshDigest();
  ++Handed_;
  return true;
}

bool TilePieces::ValidateStore(TileCell address,
                               const Generators::BakedTile &baked,
                               uint64_t sourceKey,
                               bool staged,
                               std::string &error) const {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  if (baked.RequestedDetail && sourceKey == 0) {
    error = "explicit structure detail requires a source key";
    return false;
  }
  if ((address.Cell == 0 && baked.RequestedCell) ||
      (address.Cell != 0 && (baked.RequestedCell != address.Cell || !baked.RequestedDetail))) {
    error = "structure product cell and residency address differ";
    return false;
  }
  if (staged && (address.Cell == 0 || address.Cell > Generators::kStructureCellsPerTile ||
                 sourceKey == 0 || baked.OccupiedCells != (uint64_t{1} << (address.Cell - 1u)))) {
    error = "staged structure cell needs one occupied source cell";
    return false;
  }
  if (staged && !ValidateStage(address.Tile, baked, sourceKey, error)) { return false; }
  if (!staged && address.Cell != 0) {
    const auto legacy =
        std::ranges::lower_bound(Standing_, std::pair{address.Tile, 0u}, {}, AddressOf);
    if (legacy != Standing_.end() && legacy->Tile == address.Tile && legacy->Cell == 0 &&
        legacy->Visible) {
      error = "stage structure cells before replacing a whole tile";
      return false;
    }
  }
  return true;
}

bool TilePieces::ValidateStage(uint32_t tile,
                               const Generators::BakedTile &baked,
                               uint64_t sourceKey,
                               std::string &error) const {
  const auto first = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto last = std::find_if(
      first, Standing_.end(), [tile](const Standing &held) { return held.Tile != tile; });
  const auto visible = std::find_if(first, last, [](const Standing &held) { return held.Visible; });
  const std::optional<uint64_t> visibleSource =
      visible == last ? std::nullopt : std::optional<uint64_t>{visible->SourceKey};
  for (auto at = first; at != last; ++at) {
    if (at->Visible && visibleSource != at->SourceKey) {
      error = "visible structure cells have mixed source revisions";
      return false;
    }
    if (at->Cell == baked.RequestedCell && at->SourceKey == sourceKey &&
        at->Detail == baked.RequestedDetail) {
      error = "structure cell detail is already resident";
      return false;
    }
  }
  return true;
}

void TilePieces::DiscardSupersededStage(uint32_t tile, uint64_t sourceKey) {
  const auto first = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto last = std::find_if(
      first, Standing_.end(), [tile](const Standing &held) { return held.Tile != tile; });
  const auto visible = std::find_if(first, last, [](const Standing &held) { return held.Visible; });
  const std::optional<uint64_t> visibleSource =
      visible == last ? std::nullopt : std::optional<uint64_t>{visible->SourceKey};
  for (auto at = first; at != last; ++at) {
    if (!at->Visible && at->SourceKey != sourceKey && at->SourceKey != visibleSource) {
      Releases(*at);
    }
  }
  std::erase_if(Standing_, [tile, sourceKey, visibleSource](const Standing &held) {
    return held.Tile == tile && !held.Visible && held.SourceKey != sourceKey &&
           held.SourceKey != visibleSource;
  });
}

bool TilePieces::ActivateCells(uint32_t tile,
                               CellSource source,
                               std::span<const CellSelection> selected,
                               std::string &error) {
  if (!ValidateActivation(tile, source, selected, error)) { return false; }
  const auto firstTile = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto lastTile = std::find_if(
      firstTile, Standing_.end(), [tile](const Standing &held) { return held.Tile != tile; });
  std::vector<Render::SceneRenderer::PieceRows> rows;
  rows.reserve(2u * (static_cast<size_t>(lastTile - firstTile) + selected.size()));
  std::vector<Standing *> targets;
  targets.reserve(selected.size());
  for (const CellSelection choice : selected) {
    targets.push_back(CellTarget(tile, source, choice));
  }
  for (auto at = firstTile; at != lastTile; ++at) {
    if (at->Visible && std::ranges::find(targets, &*at) == targets.end()) {
      AppendPieceRows(rows, at->Walls, at->Roofs, nullptr);
    }
  }
  for (const Standing *target : targets) {
    if (!target->Visible) { AppendPieceRows(rows, target->Walls, target->Roofs, &target->Row); }
  }
  if (!Renderer_->SetPieceInstances(rows, error)) { return false; }
  for (auto at = firstTile; at != lastTile; ++at) { at->Visible = false; }
  for (Standing *target : targets) { target->Visible = true; }
  for (auto at = firstTile; at != lastTile; ++at) {
    if (at->Cell == 0 || at->SourceKey != source.Key) { Releases(*at); }
  }
  std::erase_if(Standing_, [tile, source](const Standing &held) {
    return held.Tile == tile && (held.Cell == 0 || held.SourceKey != source.Key);
  });
  RefreshDigest();
  return true;
}

bool TilePieces::ValidateActivation(uint32_t tile,
                                    CellSource source,
                                    std::span<const CellSelection> selected,
                                    std::string &error) const {
  if (Renderer_ == nullptr || source.Key == 0 || source.Occupied == 0 ||
      selected.size() != static_cast<size_t>(std::popcount(source.Occupied))) {
    error = "structure cell activation needs a complete source selection";
    return false;
  }
  uint64_t selectedMask = 0;
  uint32_t previousCell = 0;
  for (const CellSelection choice : selected) {
    if (choice.Cell <= previousCell || choice.Cell > Generators::kStructureCellsPerTile ||
        choice.Detail > LevelOfDetail::Massed) {
      error = "structure cell selection is not ordered or has an invalid level";
      return false;
    }
    selectedMask |= uint64_t{1} << (choice.Cell - 1u);
    previousCell = choice.Cell;
  }
  if (selectedMask != source.Occupied) {
    error = "structure cell selection omits or adds a source cell";
    return false;
  }
  if (!ValidateResidentSources(tile, source, error)) { return false; }
  const auto firstTile = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto lastTile = std::find_if(
      firstTile, Standing_.end(), [tile](const Standing &held) { return held.Tile != tile; });
  for (const CellSelection choice : selected) {
    const auto first =
        std::ranges::lower_bound(firstTile, lastTile, std::pair{tile, choice.Cell}, {}, AddressOf);
    const auto last = std::find_if(
        first, lastTile, [choice](const Standing &held) { return held.Cell != choice.Cell; });
    const auto target = std::find_if(first, last, [choice, source](const Standing &held) {
      return held.Detail == choice.Detail && held.SourceKey == source.Key &&
             held.OccupiedCells == (uint64_t{1} << (choice.Cell - 1u));
    });
    if (target == last) {
      error = "required structure cell detail is not resident";
      return false;
    }
  }
  return true;
}

bool TilePieces::ValidateResidentSources(uint32_t tile,
                                         CellSource source,
                                         std::string &error) const {
  const auto first = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto last = std::find_if(
      first, Standing_.end(), [tile](const Standing &held) { return held.Tile != tile; });
  const auto visible = std::find_if(first, last, [](const Standing &held) { return held.Visible; });
  const std::optional<uint64_t> visibleSource =
      visible == last ? std::nullopt : std::optional<uint64_t>{visible->SourceKey};
  for (auto at = first; at != last; ++at) {
    if (at->Visible && visibleSource != at->SourceKey) {
      error = "visible structure cells have mixed source revisions";
      return false;
    }
    if (at->SourceKey != source.Key && at->SourceKey != visibleSource) {
      error = "staged structure cell belongs to another source revision";
      return false;
    }
    if (at->SourceKey == source.Key && at->Cell != 0 &&
        (at->Cell > Generators::kStructureCellsPerTile ||
         (source.Occupied & (uint64_t{1} << (at->Cell - 1u))) == 0)) {
      error = "resident structure cell is outside the source selection";
      return false;
    }
  }
  return true;
}

TilePieces::Standing *
TilePieces::CellTarget(uint32_t tile, CellSource source, CellSelection choice) noexcept {
  const auto first =
      std::ranges::lower_bound(Standing_, std::pair{tile, choice.Cell}, {}, AddressOf);
  const auto last = std::find_if(first, Standing_.end(), [tile, choice](const Standing &held) {
    return held.Tile != tile || held.Cell != choice.Cell;
  });
  const auto target = std::find_if(first, last, [choice, source](const Standing &held) {
    return held.Detail == choice.Detail && held.SourceKey == source.Key &&
           held.OccupiedCells == (uint64_t{1} << (choice.Cell - 1u));
  });
  return &*target;
}

void TilePieces::Forgets(uint32_t tile) {
  const auto first = std::ranges::lower_bound(Standing_, tile, {}, &Standing::Tile);
  const auto last = std::find_if(
      first, Standing_.end(), [tile](const Standing &held) { return held.Tile != tile; });
  std::for_each(first, last, [this](const Standing &held) { Releases(held); });
  Standing_.erase(first, last);
  RefreshDigest();
}

void TilePieces::ForgetsCell(uint32_t tile, uint32_t cell) {
  const auto first = std::ranges::lower_bound(Standing_, std::pair{tile, cell}, {}, AddressOf);
  const auto last = std::find_if(first, Standing_.end(), [tile, cell](const Standing &held) {
    return held.Tile != tile || held.Cell != cell;
  });
  std::for_each(first, last, [this](const Standing &held) { Releases(held); });
  Standing_.erase(first, last);
  RefreshDigest();
}

void TilePieces::ForgetsDetail(uint32_t tile, uint32_t cell, std::optional<LevelOfDetail> detail) {
  const auto first = std::ranges::lower_bound(Standing_, std::pair{tile, cell}, {}, AddressOf);
  const auto last = std::find_if(first, Standing_.end(), [tile, cell](const Standing &held) {
    return held.Tile != tile || held.Cell != cell;
  });
  const auto at =
      std::find_if(first, last, [detail](const Standing &held) { return held.Detail == detail; });
  if (at == last) { return; }
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
  return SelectDetail(tile, 0, detail, error);
}

bool TilePieces::SelectDetail(uint32_t tile,
                              uint32_t cell,
                              LevelOfDetail detail,
                              std::string &error) {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  const auto first = std::ranges::lower_bound(Standing_, std::pair{tile, cell}, {}, AddressOf);
  const auto last = std::find_if(first, Standing_.end(), [tile, cell](const Standing &held) {
    return held.Tile != tile || held.Cell != cell;
  });
  const auto target =
      std::find_if(first, last, [detail](const Standing &held) { return held.Detail == detail; });
  if (target == last) {
    error = "the requested structure detail is not resident";
    return false;
  }
  if (target->Visible) { return true; }
  const auto current = std::find_if(first, last, [](const Standing &held) { return held.Visible; });
  std::array<Render::SceneRenderer::PieceRows, 4> rows;
  size_t count = 0;
  if (current != last) {
    if (current->Walls) { rows[count++] = {.Piece = current->Walls, .Rows = {}}; }
    if (current->Roofs) { rows[count++] = {.Piece = current->Roofs, .Rows = {}}; }
  }
  if (target->Walls) { rows[count++] = {.Piece = target->Walls, .Rows = {&target->Row, 1}}; }
  if (target->Roofs) { rows[count++] = {.Piece = target->Roofs, .Rows = {&target->Row, 1}}; }
  if (!Renderer_->SetPieceInstances(std::span(rows.data(), count), error)) { return false; }
  if (current != last) { current->Visible = false; }
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
      error = "structure tile " + std::to_string(stood.Tile) + " cell " +
              std::to_string(stood.Cell) + " holds missing piece slot " +
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
    if (stood.Cell != 0) { Digest_ = (Digest_ ^ static_cast<uint64_t>(stood.Cell)) * kDigestPrime; }
    Digest_ = (Digest_ ^ stood.Digest) * kDigestPrime;
    if (stood.SourceKey != 0) { Digest_ = (Digest_ ^ stood.SourceKey) * kDigestPrime; }
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
