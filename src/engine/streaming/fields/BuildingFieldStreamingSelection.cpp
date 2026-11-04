#include "BuildingFieldStreamingState.h"
#include "SurfacePreparation.h"
#include "StructureBuildQueue.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
std::expected<SourceCoverage, std::string>
BuildingFieldStreaming::State::OriginalCoverage(const Tile &tile, uint64_t cells) {
  if (!tile.Coverage || (cells & ~tile.Source.Cells) != 0) {
    return std::unexpected("original geometry contains sources outside its metadata domain");
  }
  std::vector<SourceCoverage::Range> ranges;
  for (const auto &cell : tile.Cells) {
    if ((cells & (uint64_t{1} << (cell.Source.Cell - 1u))) == 0) { continue; }
    const auto members = cell.Coverage.Ranges();
    ranges.insert(ranges.end(), members.begin(), members.end());
  }
  std::ranges::sort(ranges, {}, &SourceCoverage::Range::First);
  auto result = SourceCoverage::FromRanges(
      std::string(tile.Coverage->Revision()), tile.Coverage->SourceCount(), ranges);
  if (!result) { return std::unexpected("original geometry source intervals overlap"); }
  return std::move(*result);
}

std::expected<std::vector<BuildingFieldStreaming::State::OriginalProduct>, std::string>
BuildingFieldStreaming::State::BindOriginals(Context &context,
                                             const Tile &tile,
                                             uint64_t sourceKey) {
  std::vector<OriginalProduct> result;
  std::string error;
  context.Pages.ForEachDigest(
      [&](const auto &page) {
        if (!error.empty() || page.Tile != tile.Index || page.SourceKey != sourceKey ||
            page.Cell == 0 || page.Detail == LevelOfDetail::Skyline) {
          return;
        }
        OriginalProduct product{.Legacy = {.Cell = page.Cell,
                                           .Detail = page.Detail,
                                           .OccupiedCells = page.OccupiedCells},
                                .Key = page.AssetKey,
                                .Coverage = page.Coverage};
        if (sourceKey == tile.SourceKey && !product.Coverage) {
          const auto sources = OriginalCoverage(tile, product.Legacy.Mask());
          if (!sources) {
            error = sources.error();
            return;
          }
          const auto bound = context.Pages.BindCoverage(
              {.Tile = tile.Index, .SourceKey = sourceKey, .AssetKey = product.Key}, *sources);
          if (!bound) {
            error = bound.error();
            return;
          }
          product.Coverage = *bound;
        }
        result.push_back(product);
      },
      Render::GeometryPasses::Shadow);
  if (!error.empty()) { return std::unexpected(std::move(error)); }
  return result;
}

BuildingFieldStreaming::State::ProductCut
BuildingFieldStreaming::State::SelectReplacements(std::span<const OriginalProduct> originals,
                                                  std::span<const Cell *const> candidates) {
  ProductCut result;
  for (const auto &original : originals) {
    result.Shadow.push_back(original.Key);
    std::vector<const Cell *> children;
    std::vector<const SourceCoverage *> sources;
    for (const auto *cell : candidates) {
      if (!original.Coverage->Contains(cell->Coverage)) { continue; }
      children.push_back(cell);
      sources.push_back(&cell->Coverage);
    }
    if (!SourceCoverage::Partition(*original.Coverage, sources)) {
      result.Main.push_back(original.Key);
      continue;
    }
    for (const auto *cell : children) {
      result.Main.push_back(cell->AssetKey);
      result.ActiveCells |= uint64_t{1} << (cell->Source.Cell - 1u);
      result.ActiveGroups += cell->Groups.size();
    }
  }
  std::ranges::sort(result.Main);
  std::ranges::sort(result.Shadow);
  return result;
}

bool BuildingFieldStreaming::State::Select(Context &context, std::string &error) {
  Statistics.Active = Statistics.ActiveGroups = Statistics.SelectedProducts = 0;
  for (auto &tile : Tiles) {
    if (!SelectTile(context, tile, error)) { return false; }
  }
  return true;
}

bool BuildingFieldStreaming::State::SelectTile(Context &context, Tile &tile, std::string &error) {
  if (!tile.Coverage.has_value()) { return true; }
  const auto &expected = tile.Coverage.value();
  const auto qualified =
      StructureBuildQueue::QualifiedSourceKey(context.Surface.Footprints(), tile.Index);
  if (!qualified) { return true; }
  auto originals = BindOriginals(context, tile, *qualified);
  if (!originals) {
    error = originals.error();
    return false;
  }
  if (originals->empty()) { return true; }
  uint64_t mask = 0;
  std::vector<Render::GeometryPageResidency::CellSelection> legacy;
  std::vector<const SourceCoverage *> originalSources;
  for (const auto &original : *originals) {
    legacy.push_back(original.Legacy);
    mask |= original.Legacy.Mask();
    originalSources.push_back(original.Coverage);
  }
  if (*qualified != tile.SourceKey) {
    if (!context.Pages.ActivateCells(
            tile.Index, {.Key = *qualified, .Occupied = mask}, legacy, error)) {
      return false;
    }
    tile.MainProducts.clear();
    tile.ShadowProducts.clear();
    tile.CandidateProducts.clear();
    tile.Active = 0;
    tile.ActiveGroups = 0;
    return true;
  }
  std::vector<std::string_view> shadowKeys;
  for (const auto &original : *originals) { shadowKeys.push_back(original.Key); }
  std::ranges::sort(shadowKeys);
  std::vector<const Cell *> candidates;
  for (const auto &cell : tile.Cells) {
    if (cell.Where == Phase::Ready && cell.Staged &&
        std::ranges::all_of(cell.Groups, [&](const Group &g) { return ValidPose(context, g); })) {
      candidates.push_back(&cell);
    }
  }
  const auto candidateKeys = candidates | std::views::transform([](const Cell *cell) {
                               return std::string_view(cell->AssetKey);
                             });
  if (std::ranges::equal(shadowKeys, tile.ShadowProducts) &&
      std::ranges::equal(candidateKeys, tile.CandidateProducts)) {
    Statistics.ActiveGroups += tile.ActiveGroups;
    Statistics.SelectedProducts += tile.MainProducts.size();
    Statistics.Active += static_cast<size_t>(std::popcount(tile.Active));
    return true;
  }
  if (!SourceCoverage::Partition(expected, originalSources)) { return true; }
  tile.Originals = std::move(legacy);
  const auto selected = SelectReplacements(*originals, candidates);
  const auto &main = selected.Main;
  const auto &shadow = selected.Shadow;
  const auto active = selected.ActiveCells;
  Statistics.ActiveGroups += selected.ActiveGroups;
  Statistics.SelectedProducts += main.size();
  Statistics.Active += static_cast<size_t>(std::popcount(active));
  if ((!std::ranges::equal(main, tile.MainProducts) ||
       !std::ranges::equal(shadow, tile.ShadowProducts)) &&
      !context.Pages.ActivateProducts(tile.Index, tile.SourceKey, expected, main, shadow, error)) {
    return false;
  }
  tile.MainProducts.assign(main.begin(), main.end());
  tile.ShadowProducts.assign(shadow.begin(), shadow.end());
  tile.CandidateProducts.assign(candidateKeys.begin(), candidateKeys.end());
  tile.Active = active;
  tile.ActiveGroups = selected.ActiveGroups;
  return true;
}
}
