#include "GeometryPageResidency.h"
#include "math/RenderFrame.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Digest.h"
#include "SceneRenderer.h"
#include "Shape.h"

namespace outshine::Render {

namespace {
constexpr uint32_t kGeometryPageCellsMost = std::numeric_limits<uint64_t>::digits;

std::expected<PieceHandle, std::string>
UploadPart(SceneRenderer &renderer, const GeometryPagePartView &part, const Mat4 &row) {
  if (part.Indices.empty()) { return PieceHandle{}; }
  if (part.Textured && (std::ranges::any_of(part.Verts,
                                            [](const StoredVertex &v) {
                                              return !std::isfinite(v.uv()[0]) ||
                                                     !std::isfinite(v.uv()[1]);
                                            }) ||
                        std::ranges::all_of(part.Verts, [](const StoredVertex &v) {
                          return v.uv()[0] == 0 && v.uv()[1] == 0;
                        }))) {
    return std::unexpected("textured geometry page has invalid surface coordinates");
  }
  return renderer.PlaceStructurePiece({.Tangents = part.Tangents,
                                       .Verts = part.Verts,
                                       .Indices = part.Indices,
                                       .Clusters = part.Clusters,
                                       .Colours = part.Colours,
                                       .Uv1 = part.Uv1,
                                       .Row = row,
                                       .Instances = {},
                                       .Surface = part.Surface,
                                       .Textured = part.Textured});
}

void AppendPieceRows(std::vector<SceneRenderer::PieceRows> &rows,
                     std::span<const PieceHandle> parts,
                     const Mat4 *row) {
  const std::span<const Mat4> instances =
      row == nullptr ? std::span<const Mat4>{} : std::span<const Mat4>{row, 1};
  for (const auto part : parts) {
    if (part) { rows.push_back({.Piece = part, .Rows = instances}); }
  }
}

}

Mat4 GeometryPageResidency::RowFor(const Vec3 &anchorEcef) const {
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

bool GeometryPageResidency::ShouldShow(uint32_t tile,
                                       uint32_t cell,
                                       std::optional<LevelOfDetail> detail,
                                       uint64_t sourceKey) const {
  if (!detail) { return true; }
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, cell}, {}, AddressOf);
  const auto last =
      std::find_if(first, Pages_.end(), [tile, cell](const ResidentGeometryPage &held) {
        return held.Tile != tile || held.Cell != cell;
      });
  if (std::any_of(first, last, [sourceKey](const ResidentGeometryPage &held) {
        return held.SourceKey != sourceKey;
      })) {
    return true;
  }
  const auto previous = std::find_if(
      first, last, [detail](const ResidentGeometryPage &held) { return held.Detail == detail; });
  if (previous != last) { return previous->Visible; }
  return std::none_of(first, last, [](const ResidentGeometryPage &held) { return held.Visible; });
}

bool GeometryPageResidency::Upload(uint32_t tile,
                                   const GeometryPageView &baked,
                                   const Vec3 &anchorEcef,
                                   std::string &error,
                                   uint64_t sourceKey) {
  return Upload(tile, 0, baked, anchorEcef, error, sourceKey);
}

bool GeometryPageResidency::Upload(uint32_t tile,
                                   uint32_t cell,
                                   const GeometryPageView &baked,
                                   const Vec3 &anchorEcef,
                                   std::string &error,
                                   uint64_t sourceKey) {
  return Store(tile, cell, baked, anchorEcef, error, sourceKey, false);
}

bool GeometryPageResidency::StageCell(uint32_t tile,
                                      uint32_t cell,
                                      const GeometryPageView &baked,
                                      const Vec3 &anchorEcef,
                                      std::string &error,
                                      uint64_t sourceKey) {
  return Store(tile, cell, baked, anchorEcef, error, sourceKey, true);
}

bool GeometryPageResidency::Store(uint32_t tile,
                                  uint32_t cell,
                                  const GeometryPageView &baked,
                                  const Vec3 &anchorEcef,
                                  std::string &error,
                                  uint64_t sourceKey,
                                  bool staged) {
  if (!ValidateStore({.Tile = tile, .Cell = cell}, baked, sourceKey, staged, error)) {
    return false;
  }
  const Mat4 row = RowFor(anchorEcef);
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, cell}, {}, AddressOf);
  const auto last =
      std::find_if(first, Pages_.end(), [tile, cell](const ResidentGeometryPage &held) {
        return held.Tile != tile || held.Cell != cell;
      });
  const bool sourceChanged =
      std::any_of(first, last, [sourceKey](const ResidentGeometryPage &held) {
        return held.SourceKey != sourceKey;
      });
  const bool visible = !staged && ShouldShow(tile, cell, baked.RequestedDetail, sourceKey);
  ResidentGeometryPage stood{.Tile = tile,
                             .Cell = cell,
                             .Digest = baked.Digest,
                             .SourceKey = sourceKey,
                             .OccupiedCells = baked.OccupiedCells,
                             .FallbackHeights = baked.FallbackHeights,
                             .Detail = baked.RequestedDetail,
                             .SelectedView = baked.SelectedView,
                             .Row = row,
                             .Visible = visible,
                             .Parts = {}};
  stood.Parts.reserve(baked.Parts.size());
  for (const auto &part : baked.Parts) {
    const auto uploaded = UploadPart(*Renderer_, part, row);
    if (!uploaded) {
      Releases(stood);
      ++Refused_;
      Why_ = uploaded.error();
      error = Why_;
      return false;
    }
    if (*uploaded) { stood.Parts.push_back(*uploaded); }
  }
  if (!visible) {
    std::vector<SceneRenderer::PieceRows> rows;
    rows.reserve(stood.Parts.size());
    AppendPieceRows(rows, stood.Parts, nullptr);
    if (!Renderer_->SetPieceInstances(rows, error)) {
      Releases(stood);
      ++Refused_;
      Why_ = error;
      return false;
    }
  }
  const WholeTileTransition retired = cell == 0 && visible
                                          ? RetireCellsForWholeTile(tile, sourceKey, error)
                                          : WholeTileTransition::NoCells;
  if (retired == WholeTileTransition::Failed) {
    Releases(stood);
    ++Refused_;
    Why_ = error;
    return false;
  }
  if (staged) { DiscardSupersededStage(tile, sourceKey); }
  if (!staged && retired != WholeTileTransition::Replaced) {
    if (baked.RequestedDetail && !sourceChanged) {
      RetireDetail(tile, cell, baked.RequestedDetail);
    } else {
      RetireCell(tile, cell);
    }
  }
  if (!stood.Parts.empty() || staged) {
    Pages_.insert(std::ranges::lower_bound(Pages_, std::pair{tile, cell}, {}, AddressOf),
                  std::move(stood));
  }
  RefreshDigest();
  ++Uploads_;
  return true;
}

GeometryPageResidency::WholeTileTransition GeometryPageResidency::RetireCellsForWholeTile(
    uint32_t tile, uint64_t sourceKey, std::string &error) {
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  if (std::none_of(first, last, [](const ResidentGeometryPage &held) { return held.Cell != 0; })) {
    return WholeTileTransition::NoCells;
  }
  std::vector<SceneRenderer::PieceRows> rows;
  size_t parts = 0;
  for (auto at = first; at != last; ++at) {
    if (at->Visible && at->Cell != 0) { parts += at->Parts.size(); }
  }
  rows.reserve(parts);
  for (auto at = first; at != last; ++at) {
    if (at->Visible && at->Cell != 0) { AppendPieceRows(rows, at->Parts, nullptr); }
  }
  if (!rows.empty() && !Renderer_->SetPieceInstances(rows, error)) {
    return WholeTileTransition::Failed;
  }
  const auto retire = [tile, sourceKey](const ResidentGeometryPage &held) {
    return held.Tile == tile && (held.Cell == 0 || held.Visible || held.SourceKey != sourceKey);
  };
  for (auto at = first; at != last; ++at) {
    if (retire(*at)) { Releases(*at); }
  }
  std::erase_if(Pages_, retire);
  RefreshDigest();
  return WholeTileTransition::Replaced;
}

bool GeometryPageResidency::ValidateStore(TileCell address,
                                          const GeometryPageView &baked,
                                          uint64_t sourceKey,
                                          bool staged,
                                          std::string &error) const {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  if ((baked.RequestedDetail || baked.RequestedCell) && sourceKey == 0) {
    error = "addressed geometry geometry requires a source key";
    return false;
  }
  if ((address.Cell == 0 && baked.RequestedCell) ||
      (address.Cell != 0 && baked.RequestedCell != address.Cell)) {
    error = "geometry product cell and residency address differ";
    return false;
  }
  if (staged && (address.Cell == 0 || address.Cell > kGeometryPageCellsMost || sourceKey == 0 ||
                 baked.OccupiedCells != (uint64_t{1} << (address.Cell - 1u)))) {
    error = "staged geometry cell needs one occupied source cell";
    return false;
  }
  if (staged && !ValidateStage(address.Tile, baked, sourceKey, error)) { return false; }
  if (!staged && address.Cell != 0) {
    const auto legacy =
        std::ranges::lower_bound(Pages_, std::pair{address.Tile, 0u}, {}, AddressOf);
    if (legacy != Pages_.end() && legacy->Tile == address.Tile && legacy->Cell == 0 &&
        legacy->Visible) {
      error = "stage geometry cells before replacing a whole tile";
      return false;
    }
  }
  return true;
}

bool GeometryPageResidency::ValidateStage(uint32_t tile,
                                          const GeometryPageView &baked,
                                          uint64_t sourceKey,
                                          std::string &error) const {
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  const auto visible =
      std::find_if(first, last, [](const ResidentGeometryPage &held) { return held.Visible; });
  const std::optional<uint64_t> visibleSource =
      visible == last ? std::nullopt : std::optional<uint64_t>{visible->SourceKey};
  for (auto at = first; at != last; ++at) {
    if (at->Visible && visibleSource != at->SourceKey) {
      error = "visible geometry cells have mixed source revisions";
      return false;
    }
    if (at->Cell == baked.RequestedCell && at->SourceKey == sourceKey &&
        at->Detail == baked.RequestedDetail) {
      error = "geometry cell detail is already resident";
      return false;
    }
  }
  return true;
}

void GeometryPageResidency::DiscardSupersededStage(uint32_t tile, uint64_t sourceKey) {
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  const auto visible =
      std::find_if(first, last, [](const ResidentGeometryPage &held) { return held.Visible; });
  const std::optional<uint64_t> visibleSource =
      visible == last ? std::nullopt : std::optional<uint64_t>{visible->SourceKey};
  for (auto at = first; at != last; ++at) {
    if (!at->Visible && at->SourceKey != sourceKey && at->SourceKey != visibleSource) {
      Releases(*at);
    }
  }
  std::erase_if(Pages_, [tile, sourceKey, visibleSource](const ResidentGeometryPage &held) {
    return held.Tile == tile && !held.Visible && held.SourceKey != sourceKey &&
           held.SourceKey != visibleSource;
  });
}

bool GeometryPageResidency::ActivateCells(uint32_t tile,
                                          CellSource source,
                                          std::span<const CellSelection> selected,
                                          std::string &error) {
  if (!ValidateActivation(tile, source, selected, error)) { return false; }
  const auto firstTile = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto lastTile =
      std::find_if(firstTile, Pages_.end(), [tile](const ResidentGeometryPage &held) {
        return held.Tile != tile;
      });
  std::vector<SceneRenderer::PieceRows> rows;
  size_t parts = 0;
  for (auto at = firstTile; at != lastTile; ++at) { parts += at->Parts.size(); }
  rows.reserve(parts);
  std::vector<ResidentGeometryPage *> targets;
  targets.reserve(selected.size());
  for (const CellSelection choice : selected) {
    targets.push_back(CellTarget(tile, source, choice));
  }
  for (auto at = firstTile; at != lastTile; ++at) {
    if (at->Visible && std::ranges::find(targets, &*at) == targets.end()) {
      AppendPieceRows(rows, at->Parts, nullptr);
    }
  }
  for (const ResidentGeometryPage *target : targets) {
    if (!target->Visible) { AppendPieceRows(rows, target->Parts, &target->Row); }
  }
  if (!Renderer_->SetPieceInstances(rows, error)) { return false; }
  for (auto at = firstTile; at != lastTile; ++at) { at->Visible = false; }
  for (ResidentGeometryPage *target : targets) { target->Visible = true; }
  for (auto at = firstTile; at != lastTile; ++at) {
    if (at->Cell == 0 || at->SourceKey != source.Key) { Releases(*at); }
  }
  std::erase_if(Pages_, [tile, source](const ResidentGeometryPage &held) {
    return held.Tile == tile && (held.Cell == 0 || held.SourceKey != source.Key);
  });
  RefreshDigest();
  return true;
}

bool GeometryPageResidency::ValidateActivation(uint32_t tile,
                                               CellSource source,
                                               std::span<const CellSelection> selected,
                                               std::string &error) const {
  if (Renderer_ == nullptr || source.Key == 0 || source.Occupied == 0 ||
      selected.size() != static_cast<size_t>(std::popcount(source.Occupied))) {
    error = "geometry cell activation needs a complete source selection";
    return false;
  }
  uint64_t selectedMask = 0;
  uint32_t previousCell = 0;
  for (const CellSelection choice : selected) {
    if (choice.Cell <= previousCell || choice.Cell > kGeometryPageCellsMost ||
        (choice.Detail && *choice.Detail > LevelOfDetail::Massed)) {
      error = "geometry cell selection is not ordered or has an invalid level";
      return false;
    }
    selectedMask |= uint64_t{1} << (choice.Cell - 1u);
    previousCell = choice.Cell;
  }
  if (selectedMask != source.Occupied) {
    error = "geometry cell selection omits or adds a source cell";
    return false;
  }
  if (!ValidateResidentSources(tile, source, error)) { return false; }
  const auto firstTile = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto lastTile =
      std::find_if(firstTile, Pages_.end(), [tile](const ResidentGeometryPage &held) {
        return held.Tile != tile;
      });
  for (const CellSelection choice : selected) {
    const auto first =
        std::ranges::lower_bound(firstTile, lastTile, std::pair{tile, choice.Cell}, {}, AddressOf);
    const auto last = std::find_if(first, lastTile, [choice](const ResidentGeometryPage &held) {
      return held.Cell != choice.Cell;
    });
    const auto target =
        std::find_if(first, last, [choice, source](const ResidentGeometryPage &held) {
          return held.Detail == choice.Detail && held.SourceKey == source.Key &&
                 held.OccupiedCells == (uint64_t{1} << (choice.Cell - 1u));
        });
    if (target == last) {
      error = "required geometry cell detail is not resident";
      return false;
    }
  }
  return true;
}

bool GeometryPageResidency::ValidateResidentSources(uint32_t tile,
                                                    CellSource source,
                                                    std::string &error) const {
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  const auto visible =
      std::find_if(first, last, [](const ResidentGeometryPage &held) { return held.Visible; });
  const std::optional<uint64_t> visibleSource =
      visible == last ? std::nullopt : std::optional<uint64_t>{visible->SourceKey};
  for (auto at = first; at != last; ++at) {
    if (at->Visible && visibleSource != at->SourceKey) {
      error = "visible geometry cells have mixed source revisions";
      return false;
    }
    if (at->SourceKey != source.Key && at->SourceKey != visibleSource) {
      error = "staged geometry cell belongs to another source revision";
      return false;
    }
    if (at->SourceKey == source.Key && at->Cell != 0 &&
        (at->Cell > kGeometryPageCellsMost ||
         (source.Occupied & (uint64_t{1} << (at->Cell - 1u))) == 0)) {
      error = "resident geometry cell is outside the source selection";
      return false;
    }
  }
  return true;
}

GeometryPageResidency::ResidentGeometryPage *
GeometryPageResidency::CellTarget(uint32_t tile, CellSource source, CellSelection choice) noexcept {
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, choice.Cell}, {}, AddressOf);
  const auto last =
      std::find_if(first, Pages_.end(), [tile, choice](const ResidentGeometryPage &held) {
        return held.Tile != tile || held.Cell != choice.Cell;
      });
  const auto target = std::find_if(first, last, [choice, source](const ResidentGeometryPage &held) {
    return held.Detail == choice.Detail && held.SourceKey == source.Key &&
           held.OccupiedCells == (uint64_t{1} << (choice.Cell - 1u));
  });
  return &*target;
}

void GeometryPageResidency::Retire(uint32_t tile) {
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  std::for_each(first, last, [this](const ResidentGeometryPage &held) { Releases(held); });
  Pages_.erase(first, last);
  RefreshDigest();
}

bool GeometryPageResidency::HasCell(uint32_t tile,
                                    uint32_t cell,
                                    LevelOfDetail detail,
                                    uint64_t sourceKey) const noexcept {
  if (cell == 0 || cell > kGeometryPageCellsMost || sourceKey == 0 ||
      detail > LevelOfDetail::Massed) {
    return false;
  }
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, cell}, {}, AddressOf);
  const auto last =
      std::find_if(first, Pages_.end(), [tile, cell](const ResidentGeometryPage &held) {
        return held.Tile != tile || held.Cell != cell;
      });
  return std::any_of(first, last, [detail, sourceKey, cell](const ResidentGeometryPage &held) {
    return held.Detail == detail && held.SourceKey == sourceKey &&
           held.OccupiedCells == (uint64_t{1} << (cell - 1u));
  });
}

bool GeometryPageResidency::HasProjectedTile(uint32_t tile,
                                             CellSource source,
                                             LongitudeLatitude eye,
                                             ProjectedErrorBudget projection,
                                             std::optional<Vec3> eyeEcef) const noexcept {
  if (source.Key == 0) { return false; }
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, uint32_t{0}}, {}, AddressOf);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  uint64_t covered = 0;
  for (auto at = first; at != last; ++at) {
    if (!at->Visible) { continue; }
    if (at->Detail || at->SourceKey != source.Key || !at->SelectedView ||
        !at->SelectedView->Contains(eye, projection, eyeEcef)) {
      return false;
    }
    if (at->Cell == 0) { return at->OccupiedCells == source.Occupied; }
    if (at->Cell > kGeometryPageCellsMost) { return false; }
    const uint64_t bit = uint64_t{1} << (at->Cell - 1u);
    if (at->OccupiedCells != bit || (covered & bit) != 0) { return false; }
    covered |= bit;
  }
  return source.Occupied != 0 && covered == source.Occupied;
}

bool GeometryPageResidency::CellsActive(uint32_t tile,
                                        std::span<const CellSelection> selected,
                                        uint64_t sourceKey) const noexcept {
  if (sourceKey == 0 || selected.empty()) { return false; }
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  size_t at = 0;
  for (auto stood = first; stood != last; ++stood) {
    if (!stood->Visible) { continue; }
    if (at == selected.size() || stood->Cell == 0 || stood->Cell > kGeometryPageCellsMost ||
        stood->Cell != selected[at].Cell || stood->Detail != selected[at].Detail ||
        stood->SourceKey != sourceKey ||
        stood->OccupiedCells != (uint64_t{1} << (stood->Cell - 1u))) {
      return false;
    }
    ++at;
  }
  return at == selected.size();
}

void GeometryPageResidency::DiscardStagedCells(uint32_t tile) {
  std::erase_if(Pages_, [this, tile](const ResidentGeometryPage &held) {
    if (held.Tile != tile || held.Visible) { return false; }
    Releases(held);
    return true;
  });
  RefreshDigest();
}

void GeometryPageResidency::RetireCell(uint32_t tile, uint32_t cell) {
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, cell}, {}, AddressOf);
  const auto last =
      std::find_if(first, Pages_.end(), [tile, cell](const ResidentGeometryPage &held) {
        return held.Tile != tile || held.Cell != cell;
      });
  std::for_each(first, last, [this](const ResidentGeometryPage &held) { Releases(held); });
  Pages_.erase(first, last);
  RefreshDigest();
}

void GeometryPageResidency::RetireDetail(uint32_t tile,
                                         uint32_t cell,
                                         std::optional<LevelOfDetail> detail) {
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, cell}, {}, AddressOf);
  const auto last =
      std::find_if(first, Pages_.end(), [tile, cell](const ResidentGeometryPage &held) {
        return held.Tile != tile || held.Cell != cell;
      });
  const auto at = std::find_if(
      first, last, [detail](const ResidentGeometryPage &held) { return held.Detail == detail; });
  if (at == last) { return; }
  Releases(*at);
  Pages_.erase(at);
  RefreshDigest();
}

void GeometryPageResidency::Releases(const ResidentGeometryPage &stood) {
  if (Renderer_ == nullptr) { return; }
  for (const auto part : stood.Parts) { Renderer_->ReleasePiece(part); }
}

bool GeometryPageResidency::SelectDetail(uint32_t tile, LevelOfDetail detail, std::string &error) {
  return SelectDetail(tile, 0, detail, error);
}

bool GeometryPageResidency::SelectDetail(uint32_t tile,
                                         uint32_t cell,
                                         LevelOfDetail detail,
                                         std::string &error) {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  const auto first = std::ranges::lower_bound(Pages_, std::pair{tile, cell}, {}, AddressOf);
  const auto last =
      std::find_if(first, Pages_.end(), [tile, cell](const ResidentGeometryPage &held) {
        return held.Tile != tile || held.Cell != cell;
      });
  const auto target = std::find_if(
      first, last, [detail](const ResidentGeometryPage &held) { return held.Detail == detail; });
  if (target == last) {
    error = "the requested geometry detail is not resident";
    return false;
  }
  if (target->Visible) { return true; }
  const auto current =
      std::find_if(first, last, [](const ResidentGeometryPage &held) { return held.Visible; });
  std::vector<SceneRenderer::PieceRows> rows;
  rows.reserve(target->Parts.size() + (current == last ? 0 : current->Parts.size()));
  if (current != last) { AppendPieceRows(rows, current->Parts, nullptr); }
  AppendPieceRows(rows, target->Parts, &target->Row);
  if (!Renderer_->SetPieceInstances(rows, error)) { return false; }
  if (current != last) { current->Visible = false; }
  target->Visible = true;
  RefreshDigest();
  return true;
}

bool GeometryPageResidency::ValidateSources(std::string &error) const {
  if (Renderer_ == nullptr) {
    error = "tile geometry requires a live world";
    return false;
  }
  for (const ResidentGeometryPage &stood : Pages_) {
    for (const PieceHandle piece : stood.Parts) {
      if (!piece || Renderer_->HasPieceSource(piece)) { continue; }
      error = "geometry tile " + std::to_string(stood.Tile) + " cell " +
              std::to_string(stood.Cell) + " holds missing piece slot " +
              std::to_string(piece.Slot) + ":" + std::to_string(piece.Generation);
      return false;
    }
  }
  return true;
}

void GeometryPageResidency::RefreshDigest() noexcept {
  if (Pages_.empty()) {
    Digest_ = 0;
    return;
  }
  Digest_ = kDigestBasis;
  for (const ResidentGeometryPage &stood : Pages_) {
    if (!stood.Visible) { continue; }
    Digest_ = (Digest_ ^ static_cast<uint64_t>(stood.Tile)) * kDigestPrime;
    if (stood.Cell != 0) { Digest_ = (Digest_ ^ static_cast<uint64_t>(stood.Cell)) * kDigestPrime; }
    Digest_ = (Digest_ ^ stood.Digest) * kDigestPrime;
    if (stood.SourceKey != 0) { Digest_ = (Digest_ ^ stood.SourceKey) * kDigestPrime; }
  }
}

void GeometryPageResidency::Clear() {
  while (!Pages_.empty()) { Retire(Pages_.back().Tile); }
  Renderer_ = nullptr;
  Digest_ = 0;
  Uploads_ = 0;
  Refused_ = 0;
  Why_.clear();
}

}
