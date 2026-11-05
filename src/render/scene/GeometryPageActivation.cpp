#include "GeometryPageResidency.h"
#include "SceneRenderer.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstddef>
#include <format>
#include <optional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr uint32_t kGeometryPageCellsMost = std::numeric_limits<uint64_t>::digits;

std::optional<GeometryPasses> Intersection(GeometryPasses original, GeometryPasses selected) {
  if (original == GeometryPasses::MainAndShadow) { return selected; }
  if (selected == GeometryPasses::MainAndShadow || original == selected) { return original; }
  return std::nullopt;
}

template <typename Page>
void AppendSelectionRows(std::vector<SceneRenderer::PieceRows> &rows,
                         Page &page,
                         std::optional<GeometryPasses> selected) {
  if (selected.has_value() == page.Visible && (!selected || *selected == page.Passes)) { return; }
  for (size_t part = 0; part < page.Parts.size(); ++part) {
    const auto passes = selected ? Intersection(page.Parts[part].Passes, *selected) : std::nullopt;
    rows.push_back({.Piece = page.Parts[part].Handle,
                    .Rows = passes ? std::span<const Mat4>{&page.Row, 1} : std::span<const Mat4>{},
                    .Passes = passes.value_or(page.Parts[part].Passes)});
  }
}

}

bool GeometryPageResidency::IndependentPasses(uint32_t tile) const noexcept {
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  for (auto at = first; at != Pages_.end() && at->Tile == tile; ++at) {
    if (at->Visible && at->Passes != GeometryPasses::MainAndShadow) { return true; }
  }
  return false;
}

std::vector<GeometryPageResidency::PageSelection>
GeometryPageResidency::SelectedPages(uint32_t tile,
                                     CellSource source,
                                     std::span<const CellSelection> main,
                                     std::span<const CellSelection> shadow) {
  const bool shared = main.data() == shadow.data() && main.size() == shadow.size();
  std::vector<PageSelection> targets;
  targets.reserve(shared ? main.size() : main.size() + shadow.size());
  for (const auto choice : main) {
    targets.push_back({.Page = CellTarget(tile, source, choice),
                       .Passes = shared ? GeometryPasses::MainAndShadow : GeometryPasses::Main});
  }
  for (const auto choice : shared ? std::span<const CellSelection>{} : shadow) {
    auto *page = CellTarget(tile, source, choice);
    const auto held = std::ranges::find(targets, page, &PageSelection::Page);
    if (held == targets.end()) {
      targets.push_back({.Page = page, .Passes = GeometryPasses::Shadow});
    } else {
      held->Passes = GeometryPasses::MainAndShadow;
    }
  }
  return targets;
}

bool GeometryPageResidency::ActivateCells(uint32_t tile,
                                          CellSource source,
                                          std::span<const CellSelection> selected,
                                          std::string &error) {
  return ActivateCells(tile, source, selected, selected, error);
}

bool GeometryPageResidency::ActivateCells(uint32_t tile,
                                          CellSource source,
                                          std::span<const CellSelection> main,
                                          std::span<const CellSelection> shadow,
                                          std::string &error) {
  const bool shared = main.data() == shadow.data() && main.size() == shadow.size();
  if (!ValidateActivation(tile, source, main, error) ||
      (!shared && !ValidateActivation(tile, source, shadow, error))) {
    return false;
  }
  auto targets = SelectedPages(tile, source, main, shadow);
  std::ranges::sort(targets, {}, &PageSelection::Page);
  return PublishSelection(tile, source.Key, targets, error);
}

bool GeometryPageResidency::ActivateProducts(uint32_t tile,
                                             uint64_t sourceKey,
                                             const SourceCoverage &expected,
                                             std::span<const std::string_view> main,
                                             std::span<const std::string_view> shadow,
                                             std::string &error) {
  const auto ordered = [](auto keys) {
    return std::adjacent_find(keys.begin(), keys.end(), [](auto a, auto b) { return a >= b; }) ==
           keys.end();
  };
  if (Renderer_ == nullptr || sourceKey == 0 || main.empty() || shadow.empty() || !ordered(main) ||
      !ordered(shadow)) {
    error = "geometry product activation needs ordered unique keys and complete pass selections";
    return false;
  }
  std::vector<PageSelection> targets;
  std::vector<const SourceCoverage *> mainSources;
  std::vector<const SourceCoverage *> shadowSources;
  targets.reserve(main.size() + shadow.size());
  mainSources.reserve(main.size());
  shadowSources.reserve(shadow.size());
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  for (auto at = first; at != Pages_.end() && at->Tile == tile; ++at) {
    if (at->SourceKey != sourceKey) { continue; }
    const bool inMain = std::ranges::binary_search(main, std::string_view(at->AssetKey));
    const bool inShadow = std::ranges::binary_search(shadow, std::string_view(at->AssetKey));
    if (!inMain && !inShadow) { continue; }
    if (!at->Coverage) {
      error = "selected geometry product has no exact source coverage";
      return false;
    }
    if (inMain) { mainSources.push_back(&*at->Coverage); }
    if (inShadow) { shadowSources.push_back(&*at->Coverage); }
    auto passes = GeometryPasses::MainAndShadow;
    if (!inMain) { passes = GeometryPasses::Shadow; }
    if (!inShadow) { passes = GeometryPasses::Main; }
    targets.push_back({.Page = &*at, .Passes = passes});
  }
  if (mainSources.size() != main.size() || shadowSources.size() != shadow.size()) {
    error = std::format("geometry product residency mismatch: tile={} main={}/{} shadow={}/{}",
                        tile,
                        mainSources.size(),
                        main.size(),
                        shadowSources.size(),
                        shadow.size());
    return false;
  }
  if (!SourceCoverage::Partition(expected, mainSources) ||
      !SourceCoverage::Partition(expected, shadowSources)) {
    error = "geometry products do not cover their complete revision in both passes";
    return false;
  }
  return PublishSelection(tile, sourceKey, targets, error);
}

bool GeometryPageResidency::PublishSelection(uint32_t tile,
                                             uint64_t sourceKey,
                                             std::span<const PageSelection> targets,
                                             std::string &error) {
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  const auto last = std::find_if(
      first, Pages_.end(), [tile](const ResidentGeometryPage &held) { return held.Tile != tile; });
  size_t parts = 0;
  for (auto at = first; at != last; ++at) { parts += at->Parts.size(); }
  std::vector<SceneRenderer::PieceRows> rows;
  rows.reserve(parts);
  auto selected = targets.begin();
  for (auto at = first; at != last; ++at) {
    const bool chosen = selected != targets.end() && selected->Page == &*at;
    AppendSelectionRows(rows, *at, chosen ? std::optional{selected->Passes} : std::nullopt);
    if (chosen) { ++selected; }
  }
  if (!Renderer_->SetPieceInstances(rows, error)) { return false; }
  for (auto at = first; at != last; ++at) {
    at->Visible = false;
    at->Passes = GeometryPasses::MainAndShadow;
  }
  for (const auto &target : targets) {
    target.Page->Visible = true;
    target.Page->Passes = target.Passes;
  }
  for (auto at = first; at != last; ++at) {
    if ((at->Cell == 0 && at->Mode != Publication::Product) || at->SourceKey != sourceKey) {
      Releases(*at);
    }
  }
  std::erase_if(Pages_, [tile, sourceKey](const ResidentGeometryPage &held) {
    return held.Tile == tile &&
           ((held.Cell == 0 && held.Mode != Publication::Product) || held.SourceKey != sourceKey);
  });
  RefreshDigest();
  return true;
}

bool GeometryPageResidency::ValidateActivation(uint32_t tile,
                                               CellSource source,
                                               std::span<const CellSelection> selected,
                                               std::string &error) const {
  if (Renderer_ == nullptr || source.Key == 0 || source.Occupied == 0 || selected.empty()) {
    error = "geometry cell activation needs a complete source selection";
    return false;
  }
  uint64_t selectedMask = 0;
  uint32_t previousCell = 0;
  for (const CellSelection choice : selected) {
    const uint64_t cells = choice.Mask();
    if (choice.Cell <= previousCell || choice.Cell > kGeometryPageCellsMost || cells == 0 ||
        choice.Cell != static_cast<uint32_t>(std::countr_zero(cells)) + 1u ||
        (selectedMask & cells) != 0 || (choice.Detail && *choice.Detail > LevelOfDetail::Skyline)) {
      error = "geometry cell selection is not ordered or has an invalid level";
      return false;
    }
    selectedMask |= cells;
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
                 held.OccupiedCells == choice.Mask();
        });
    if (target == last) {
      error = "required geometry cell detail is not resident";
      return false;
    }
    if (target->Coverage &&
        ((choice.ExpectedSources != nullptr && *target->Coverage != *choice.ExpectedSources) ||
         (choice.ExpectedSources == nullptr && target->CoverageFrom == CoverageSource::Product))) {
      error = "geometry product does not cover its declared source selection";
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
        (at->Cell > kGeometryPageCellsMost || at->OccupiedCells == 0 ||
         (source.Occupied & at->OccupiedCells) != at->OccupiedCells)) {
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
           held.OccupiedCells == choice.Mask();
  });
  return &*target;
}

}
