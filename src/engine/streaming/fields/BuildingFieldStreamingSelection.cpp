#include "BuildingFieldStreamingState.h"
#include "SurfacePreparation.h"
#include "StructureBuildQueue.h"
#include "SceneRenderer.h"
#include "Vec2.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace {
Box CasterBounds(const TangentFrame &frame, const Box &world) {
  Box scene;
  if (world.Empty()) { return scene; }
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto local = frame.ToLocalPosition(world.Corner(corner));
    scene.Cover(Vec3{{local.EastM, local.UpM, -local.NorthM}});
  }
  return scene;
}

bool OriginalSupportsView(const Render::GeometryPageResidency::DigestRecord &page,
                          uint64_t sourceKey,
                          const Generators::Osm::BuildingField &prints,
                          LongitudeLatitude eye,
                          const Vec3 &worldEye) {
  if (page.Detail && page.Detail != LevelOfDetail::Fine &&
      (page.ErrorBound == nullptr ||
       !page.ErrorBound->Allows(sourceKey, worldEye, prints.Projection()))) {
    return false;
  }
  return page.View == nullptr || page.View->Contains(eye, prints.Projection(), prints.EyeEcef());
}
}

std::expected<SourceCoverage, std::string>
BuildingFieldStreaming::State::OriginalCoverage(const Tile &tile, uint64_t cells) {
  if (!tile.Coverage || (cells & ~tile.Source.Cells) != 0) {
    return std::unexpected("original geometry contains sources outside its metadata domain");
  }
  if (cells == tile.Source.Cells) { return *tile.Coverage; }
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
  std::optional<OriginalProduct> whole;
  uint64_t collected = 0;
  std::string error;
  const Vec3 worldEye = WorldCamera(context).EyeM;
  const auto geographicEye = GeoFromEcef(worldEye);
  if (!geographicEye) { return result; }
  const LongitudeLatitude eye{.LongitudeDeg = geographicEye->LongitudeDeg,
                              .LatitudeDeg = geographicEye->LatitudeDeg};
  context.Pages.ForEachTileDigest(
      tile.Index,
      [&](const auto &page) {
        if (!error.empty() || page.SourceKey != sourceKey ||
            page.Detail == LevelOfDetail::Skyline) {
          return;
        }
        const auto &prints = context.Surface.Footprints();
        if (!OriginalSupportsView(page, sourceKey, prints, eye, worldEye)) { return; }
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
        if (page.Cell == 0) {
          if (!whole) { whole = product; }
        } else if ((collected & product.Legacy.Mask()) == 0) {
          collected |= product.Legacy.Mask();
          result.push_back(product);
        }
      },
      Render::GeometryPasses::MainAndShadow,
      true);
  if (!error.empty()) { return std::unexpected(std::move(error)); }
  if (result.empty() && whole) { result.push_back(*whole); }
  return result;
}

std::optional<BuildingFieldStreaming::State::ProductCut>
BuildingFieldStreaming::State::SelectReplacements(const SourceCoverage &domain,
                                                  std::span<const OriginalProduct> originals,
                                                  std::span<const Cell *const> candidates,
                                                  uint64_t shadowCells,
                                                  const Tile::RoofProduct *roof,
                                                  bool roofCaster) {
  ProductCut result;
  if (roofCaster && roof != nullptr && roof->Caster) {
    result.Shadow.push_back(roof->Caster->Key);
    result.ShadowSources.push_back(&roof->Coverage);
    result.RoofCasterActive = true;
    shadowCells &= ~roof->Cells;
  }
  for (const auto &original : originals) {
    if ((original.Legacy.Mask() & shadowCells) != 0) {
      result.Shadow.push_back(original.Key);
      result.ShadowSources.push_back(original.Coverage);
    }
  }
  std::vector<const SourceCoverage *> fallback;
  std::vector<const SourceCoverage *> ready;
  fallback.reserve(originals.size());
  ready.reserve(candidates.size() + (roof != nullptr ? 1u : 0u));
  for (const auto &original : originals) { fallback.push_back(original.Coverage); }
  for (const auto *cell : candidates) { ready.push_back(&cell->Coverage); }
  if (roof != nullptr) { ready.push_back(&roof->Coverage); }
  const auto cut = SourceCoverage::Prefer(domain, fallback, ready);
  if (!cut) { return std::nullopt; }
  for (const size_t at : cut->Retained) {
    result.Main.push_back(originals[at].Key);
    result.MainSources.push_back(originals[at].Coverage);
  }
  for (const size_t at : cut->Preferred) {
    if (at == candidates.size() && roof != nullptr) {
      result.Main.push_back(roof->Key);
      result.MainSources.push_back(&roof->Coverage);
      result.RoofActive = true;
    } else {
      const auto *cell = candidates[at];
      result.Main.push_back(cell->AssetKey);
      result.MainSources.push_back(&cell->Coverage);
      result.ActiveCells |= uint64_t{1} << (cell->Source.Cell - 1u);
      result.ActiveGroups += cell->Groups.size();
    }
  }
  std::ranges::sort(result.Main);
  std::ranges::sort(result.Shadow);
  return result;
}

const BuildingFieldStreaming::State::Tile::RoofProduct *BuildingFieldStreaming::State::EligibleRoof(
    const Context &context, const Tile &tile, std::span<const OriginalProduct> originals) {
  if (!tile.ResidentRoof || std::ranges::any_of(originals, [](const auto &original) {
        return original.Legacy.Cell == 0;
      })) {
    return nullptr;
  }
  const Vec2 viewport{{context.Renderer.PictureW(), context.Renderer.PictureH()}};
  return tile.ResidentRoof->Projected.Allows(
             WorldCamera(context).EyeM, viewport, context.Surface.Footprints().Projection())
             ? &*tile.ResidentRoof
             : nullptr;
}

bool BuildingFieldStreaming::State::Select(Context &context, std::string &error) {
  Statistics.Active = Statistics.ActiveGroups = Statistics.SelectedProducts =
      Statistics.FieldOnlyCells = 0;
  for (auto &tile : Tiles) {
    if (!SelectTile(context, tile, error)) { return false; }
  }
  return true;
}

std::vector<const BuildingFieldStreaming::State::Cell *>
BuildingFieldStreaming::State::EvaluateCells(Context &context, Tile &tile) {
  std::vector<const Cell *> candidates;
  tile.ShadowCells = 0;
  for (auto &cell : tile.Cells) {
    cell.Qualified =
        cell.Where == Phase::Ready && cell.Staged &&
        std::ranges::all_of(cell.Groups, [&](Group &g) { return ValidPose(context, g); });
    cell.ShadowNeeded = !Shadows || cell.Where != Phase::Ready ||
                        std::ranges::any_of(cell.Groups, [&](const Group &g) {
                          return !g.SceneBounds || Shadows->MayContain(*g.SceneBounds);
                        });
    if (!cell.ShadowNeeded && Shadows->Regions != 0) {
      cell.ShadowNeeded = Shadows->MayContain(CasterBounds(context.Frame, cell.Source.Bounds));
    }
    if (cell.ShadowNeeded) { tile.ShadowCells |= uint64_t{1} << (cell.Source.Cell - 1u); }
    if (cell.Qualified) {
      candidates.push_back(&cell);
      Statistics.FieldOnlyCells += cell.ShadowNeeded ? 0u : 1u;
    }
  }
  return candidates;
}

bool BuildingFieldStreaming::State::SelectTile(Context &context, Tile &tile, std::string &error) {
  tile.Complete = false;
  tile.ResidentOriginals = 0;
  tile.RoofOriginalsReplaced = 0;
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
  uint64_t mask = 0;
  std::vector<Render::GeometryPageResidency::CellSelection> legacy;
  for (const auto &original : *originals) {
    legacy.push_back(original.Legacy);
    mask |= original.Legacy.Mask();
  }
  tile.ResidentOriginals = mask;
  if (*qualified != tile.SourceKey) {
    if (!legacy.empty() && legacy.front().Cell != 0 &&
        !context.Pages.ActivateCells(
            tile.Index, {.Key = *qualified, .Occupied = mask}, legacy, error)) {
      return false;
    }
    tile.MainProducts.clear();
    tile.ShadowProducts.clear();
    tile.CandidateProducts.clear();
    tile.OriginalProducts.clear();
    tile.Active = 0;
    tile.ActiveGroups = 0;
    tile.RoofActive = tile.RoofQualified = false;
    tile.RoofCasterActive = false;
    return true;
  }
  auto candidates = EvaluateCells(context, tile);
  const auto *roof = EligibleRoof(context, tile, *originals);
  if (roof != nullptr) {
    std::erase_if(candidates, [roof](const Cell *cell) {
      return (roof->Cells & (uint64_t{1} << (cell->Source.Cell - 1u))) != 0;
    });
  }
  std::vector<std::string_view> shadowKeys;
  for (const auto &original : *originals) { shadowKeys.push_back(original.Key); }
  std::ranges::sort(shadowKeys);
  for (const auto &original : *originals) {
    if ((original.Legacy.Mask() & tile.ShadowCells) != 0) {
      tile.ShadowCells |= original.Legacy.Mask();
    }
  }
  const bool roofCaster =
      roof != nullptr && roof->Caster && (tile.ShadowCells & roof->Cells) != 0 &&
      roof->Caster->Error.Allows(
          tile.SourceKey, WorldCamera(context).EyeM, context.Surface.Footprints().Projection()) &&
      std::ranges::none_of(*originals, [roof](const OriginalProduct &original) {
        return (original.Legacy.Mask() & roof->Cells) != 0 && original.Legacy.Mask() != roof->Cells;
      });
  if (roofCaster) { tile.RoofOriginalsReplaced = roof->Cells; }
  const auto candidateKeys = candidates | std::views::transform([](const Cell *cell) {
                               return std::string_view(cell->AssetKey);
                             });
  if (!tile.MainProducts.empty() && tile.ShadowCells == tile.PublishedShadowCells &&
      tile.RoofQualified == (roof != nullptr) && tile.RoofCasterActive == roofCaster &&
      std::ranges::equal(shadowKeys, tile.OriginalProducts) &&
      std::ranges::equal(candidateKeys, tile.CandidateProducts)) {
    tile.Complete = true;
    Statistics.ActiveGroups += tile.ActiveGroups;
    Statistics.SelectedProducts += tile.MainProducts.size();
    Statistics.Active += static_cast<size_t>(std::popcount(tile.Active));
    return true;
  }
  tile.Originals = std::move(legacy);
  const auto selected =
      SelectReplacements(expected, *originals, candidates, tile.ShadowCells, roof, roofCaster);
  const auto shadowExpected = OriginalCoverage(tile, tile.ShadowCells);
  if (!selected || !shadowExpected || !SourceCoverage::Partition(expected, selected->MainSources) ||
      !SourceCoverage::Partition(*shadowExpected, selected->ShadowSources)) {
    return true;
  }
  const auto &main = selected->Main;
  const auto &shadow = selected->Shadow;
  const auto active = selected->ActiveCells;
  Statistics.ActiveGroups += selected->ActiveGroups;
  Statistics.SelectedProducts += main.size();
  Statistics.Active += static_cast<size_t>(std::popcount(active));
  if ((!std::ranges::equal(main, tile.MainProducts) ||
       !std::ranges::equal(shadow, tile.ShadowProducts)) &&
      !context.Pages.ActivateProducts(tile.Index,
                                      tile.SourceKey,
                                      {.Expected = expected, .Keys = main},
                                      {.Expected = *shadowExpected, .Keys = shadow},
                                      error)) {
    return false;
  }
  tile.MainProducts.assign(main.begin(), main.end());
  tile.ShadowProducts.assign(shadow.begin(), shadow.end());
  tile.CandidateProducts.assign(candidateKeys.begin(), candidateKeys.end());
  tile.OriginalProducts.assign(shadowKeys.begin(), shadowKeys.end());
  tile.PublishedShadowCells = tile.ShadowCells;
  tile.Complete = true;
  tile.Active = active;
  tile.ActiveGroups = selected->ActiveGroups;
  tile.RoofActive = selected->RoofActive;
  tile.RoofQualified = roof != nullptr;
  tile.RoofCasterActive = selected->RoofCasterActive;
  return true;
}
}
