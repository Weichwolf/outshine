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
#include "RenderGeometryView.h"

namespace outshine::Render {

namespace {
constexpr uint32_t kGeometryPageCellsMost = std::numeric_limits<uint64_t>::digits;

std::expected<PieceHandle, std::string> UploadPart(SceneRenderer &renderer,
                                                   const GeometryPagePartView &part,
                                                   const Mat4 &row,
                                                   std::array<uint64_t, 3> productOrder) {
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
                                       .Textured = part.Textured,
                                       .ProductOrder = productOrder,
                                       .Passes = part.Passes});
}

void AppendPieceRows(std::vector<SceneRenderer::PieceRows> &rows,
                     const auto &parts,
                     const Mat4 *row) {
  const std::span<const Mat4> instances =
      row == nullptr ? std::span<const Mat4>{} : std::span<const Mat4>{row, 1};
  for (const auto &part : parts) {
    if (part.Handle) { rows.push_back({.Piece = part.Handle, .Rows = instances}); }
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
  return Store(tile, cell, baked, anchorEcef, error, sourceKey, Publication::Immediate);
}

bool GeometryPageResidency::StageCell(uint32_t tile,
                                      uint32_t cell,
                                      const GeometryPageView &baked,
                                      const Vec3 &anchorEcef,
                                      std::string &error,
                                      uint64_t sourceKey) {
  if (cell == 0 || cell > kGeometryPageCellsMost ||
      baked.OccupiedCells != (uint64_t{1} << (cell - 1u))) {
    error = "staged geometry cell needs one occupied source cell";
    return false;
  }
  return Store(tile, cell, baked, anchorEcef, error, sourceKey, Publication::Cell);
}

bool GeometryPageResidency::StageCells(uint32_t tile,
                                       const GeometryPageView &baked,
                                       const Vec3 &anchorEcef,
                                       std::string &error,
                                       uint64_t sourceKey) {
  const uint64_t cells = baked.OccupiedCells;
  if (cells == 0) {
    error = "staged geometry cohort needs its complete source cell mask";
    return false;
  }
  const auto first = static_cast<uint32_t>(std::countr_zero(cells)) + 1u;
  return Store(tile, first, baked, anchorEcef, error, sourceKey, Publication::Cohort);
}

bool GeometryPageResidency::Store(uint32_t tile,
                                  uint32_t cell,
                                  const GeometryPageView &baked,
                                  const Vec3 &anchorEcef,
                                  std::string &error,
                                  uint64_t sourceKey,
                                  Publication publication) {
  const bool staged = publication != Publication::Immediate;
  if (!ValidateStore({.Tile = tile, .Cell = cell}, baked, sourceKey, publication, error)) {
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
  for (size_t at = 0; at < baked.Parts.size(); ++at) {
    const auto uploaded =
        UploadPart(*Renderer_, baked.Parts[at], row, {sourceKey, baked.Digest, at});
    if (!uploaded) {
      Releases(stood);
      ++Refused_;
      Why_ = uploaded.error();
      error = Why_;
      return false;
    }
    if (*uploaded) {
      stood.Parts.push_back({.Handle = *uploaded, .Passes = baked.Parts[at].Passes});
    }
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
  if (staged) {
    DiscardSupersededStage(tile, sourceKey);
    DiscardStagedRepresentation({.Tile = tile, .Cell = cell}, baked, sourceKey);
  }
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

void GeometryPageResidency::DiscardStagedRepresentation(TileCell address,
                                                        const GeometryPageView &baked,
                                                        uint64_t sourceKey) {
  std::erase_if(Pages_, [&](const ResidentGeometryPage &held) {
    if (held.Tile != address.Tile || held.Cell != address.Cell || held.Visible ||
        held.SourceKey != sourceKey || held.Detail != baked.RequestedDetail ||
        held.OccupiedCells != baked.OccupiedCells) {
      return false;
    }
    Releases(held);
    return true;
  });
}

bool GeometryPageResidency::ValidateStore(TileCell address,
                                          const GeometryPageView &baked,
                                          uint64_t sourceKey,
                                          Publication publication,
                                          std::string &error) const {
  const bool staged = publication != Publication::Immediate;
  if (Renderer_ == nullptr || address.Cell > kGeometryPageCellsMost) {
    error = "tile geometry requires a live world and a valid cell";
    return false;
  }
  if ((baked.RequestedDetail || baked.RequestedCell) && sourceKey == 0) {
    error = "addressed geometry requires a source key";
    return false;
  }
  const auto addressedCell = address.Cell == 0 ? std::nullopt : std::optional{address.Cell};
  if (baked.RequestedCell != addressedCell) {
    error = "geometry product cell and residency address differ";
    return false;
  }
  if (staged &&
      (address.Cell == 0 || address.Cell > kGeometryPageCellsMost || sourceKey == 0 ||
       baked.OccupiedCells == 0 ||
       address.Cell != static_cast<uint32_t>(std::countr_zero(baked.OccupiedCells)) + 1u)) {
    error = "staged geometry cohort needs its canonical source cell anchor";
    return false;
  }
  if (staged && !ValidateStage(address.Tile, baked, sourceKey, publication, error)) {
    return false;
  }
  if (!staged && address.Cell != 0) {
    const uint64_t cell = uint64_t{1} << (address.Cell - 1u);
    if (baked.OccupiedCells != 0 && baked.OccupiedCells != cell) {
      error = "stage multi-cell geometry before activating its complete source";
      return false;
    }
    const auto legacy =
        std::ranges::lower_bound(Pages_, std::pair{address.Tile, 0u}, {}, AddressOf);
    if (legacy != Pages_.end() && legacy->Tile == address.Tile && legacy->Cell == 0 &&
        legacy->Visible) {
      error = "stage geometry cells before replacing a whole tile";
      return false;
    }
    if (IndependentPasses(address.Tile) ||
        std::ranges::any_of(Pages_, [address, cell](const ResidentGeometryPage &held) {
          return held.Tile == address.Tile && held.Visible && held.Cell != 0 &&
                 (held.OccupiedCells & cell) != 0 && held.OccupiedCells != cell;
        })) {
      error = "stage complete geometry selections before replacing cohorts or independent passes";
      return false;
    }
  }
  return true;
}

bool GeometryPageResidency::ValidateStage(uint32_t tile,
                                          const GeometryPageView &baked,
                                          uint64_t sourceKey,
                                          Publication publication,
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
        at->Detail == baked.RequestedDetail && at->OccupiedCells == baked.OccupiedCells &&
        (publication != Publication::Cohort || at->Digest == baked.Digest)) {
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
    if (!at->Visible || at->Passes == GeometryPasses::Shadow) { continue; }
    if (at->Detail || at->SourceKey != source.Key || !at->SelectedView ||
        !at->SelectedView->Contains(eye, projection, eyeEcef)) {
      return false;
    }
    if (at->Cell == 0) { return at->OccupiedCells == source.Occupied; }
    if (at->Cell > kGeometryPageCellsMost) { return false; }
    if (at->OccupiedCells == 0 || (covered & at->OccupiedCells) != 0) { return false; }
    covered |= at->OccupiedCells;
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
    if (!stood->Visible || stood->Passes == GeometryPasses::Shadow) { continue; }
    if (at == selected.size() || stood->Cell == 0 || stood->Cell > kGeometryPageCellsMost ||
        stood->Cell != selected[at].Cell || stood->Detail != selected[at].Detail ||
        stood->SourceKey != sourceKey || stood->OccupiedCells != selected[at].Mask()) {
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
  for (const auto &part : stood.Parts) { Renderer_->ReleasePiece(part.Handle); }
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
  if (cell > kGeometryPageCellsMost || IndependentPasses(tile) ||
      std::ranges::any_of(Pages_, [tile, cell](const ResidentGeometryPage &held) {
        return held.Tile == tile && held.Visible && held.Cell != 0 && cell != 0 &&
               (held.OccupiedCells & (uint64_t{1} << (cell - 1u))) != 0 &&
               held.OccupiedCells != (uint64_t{1} << (cell - 1u));
      })) {
    error = "activate complete geometry cohorts before selecting one cell";
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
    for (const auto &part : stood.Parts) {
      const PieceHandle piece = part.Handle;
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
    if (stood.Cell != 0 && stood.OccupiedCells != 0 && !std::has_single_bit(stood.OccupiedCells)) {
      Digest_ = (Digest_ ^ stood.OccupiedCells) * kDigestPrime;
    }
    if (stood.Passes != GeometryPasses::MainAndShadow) {
      Digest_ = (Digest_ ^ static_cast<uint64_t>(stood.Passes)) * kDigestPrime;
    }
    Digest_ = (Digest_ ^ stood.Digest) * kDigestPrime;
    if (stood.SourceKey != 0) { Digest_ = (Digest_ ^ stood.SourceKey) * kDigestPrime; }
    if (stood.FallbackHeights) { Digest_ = (Digest_ ^ uint64_t{1}) * kDigestPrime; }
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
