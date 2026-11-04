#include "BuildingFieldStreamingState.h"
#include "SurfacePreparation.h"
#include "StructureBuildQueue.h"
#include "PageIndexAsset.h"
#include "Tasks.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
void BuildingFieldStreaming::State::PrepareCells(
    Tile &tile,
    const Generators::PreparedBuildingAssets::SourceGroups &groups,
    const SourceProductSelection &selection,
    const SourceProductSelection::Cut &cut) {
  for (const auto &source : groups.Cells) {
    Cell cell;
    cell.Source = source;
    tile.Cells.push_back(std::move(cell));
  }
  for (const auto &product : cut.Products) {
    const auto members = selection.Members(cut, product);
    if (!members || members->empty()) { continue; }
    const auto cellId = groups.Members[(*members)[0]].Cell;
    const auto cell =
        std::ranges::find(tile.Cells, cellId, [](const auto &c) { return c.Source.Cell; });
    if (cell == tile.Cells.end()) { continue; }
    if (product.Representation != SourceProductSelection::Kind::MaterialField) {
      cell->Where = Phase::Rejected;
      continue;
    }
    Group group;
    group.Sources.assign(members->begin(), members->end());
    group.Bounds = selection.Bounds(product);
    cell->Groups.push_back(std::move(group));
  }
  for (auto &cell : tile.Cells) {
    size_t covered = 0;
    for (const auto &group : cell.Groups) { covered += group.Sources.size(); }
    if (cell.Where != Phase::Rejected && !cell.Groups.empty() &&
        covered == tile.Source.CellSourceCounts[cell.Source.Cell - 1u]) {
      cell.Alternatives = std::move(cell.Groups);
    }
    cell.Groups.clear();
    Group coarse;
    coarse.Bounds = cell.Source.Bounds;
    cell.Groups.push_back(std::move(coarse));
    cell.Where = Phase::Waiting;
  }
}

void BuildingFieldStreaming::State::PollMetadata() {
  if (!Reading || !Reading->Done.load(std::memory_order_acquire)) { return; }
  if (Reading->Groups && Reading->Selection && Reading->Cut && ReadingTile) {
    PrepareCells(*ReadingTile, *Reading->Groups, *Reading->Selection, *Reading->Cut);
    Tiles.push_back(std::move(*ReadingTile));
  }
  ReadingTile.reset();
  Reading.reset();
}

void BuildingFieldStreaming::State::StartMetadata(Context &context) {
  const auto entries = Sources.Entries();
  if (Reading || entries.empty()) { return; }
  const auto at = NextSource++ % entries.size();
  const auto &entry = entries[at];
  if (!entry.Value || entry.Where != BuildingSourceDemand::Phase::Ready ||
      !entry.Value->HeightQualified ||
      std::ranges::any_of(Tiles, [at](const auto &tile) { return tile.Index == at; })) {
    return;
  }
  const auto qualified = StructureBuildQueue::QualifiedSourceKey(context.Surface.Footprints(),
                                                                 static_cast<uint32_t>(at));
  if (!qualified) { return; }
  Tile tile{.Index = static_cast<uint32_t>(at),
            .SourceKey = *qualified,
            .Source = *entry.Value,
            .Originals = {},
            .Cells = {}};
  context.Pages.ForEachDigest(
      [&](const auto &page) {
        if (page.Tile == at && page.SourceKey == tile.SourceKey && page.Cell != 0 &&
            page.Detail != LevelOfDetail::Skyline) {
          tile.Originals.push_back(
              {.Cell = page.Cell, .Detail = page.Detail, .OccupiedCells = page.OccupiedCells});
        }
      },
      Render::GeometryPasses::Shadow);
  uint64_t covered = 0;
  for (const auto &choice : tile.Originals) { covered |= choice.Mask(); }
  if (covered != tile.Source.Cells) { return; }
  const size_t count = std::accumulate(
      tile.Source.CellSourceCounts.begin(), tile.Source.CellSourceCounts.end(), size_t{0});
  const auto bytes = PageIndexAsset::EncodedBytes(count);
  if (!bytes) { return; }
  auto result = std::make_shared<Metadata>();
  const auto assets = context.Surface.BuildingAssets();
  const auto key = tile.Source.BaseKey;
  const auto cells = tile.Source.Cells;
  const auto eye = WorldCamera(context).EyeM;
  const auto projection = context.Surface.Footprints().Projection();
  if (!context.Workers.PostDetached([assets, key, cells, result, bytes, eye, projection] {
        auto groups =
            assets->DescribeSourceGroups(key, {.Occupied = cells, .IndexBytesMost = *bytes});
        if (groups && *groups) {
          std::vector<SourceProductSelection::Source> traits;
          traits.reserve((**groups).Members.size());
          for (const auto &member : (**groups).Members) {
            traits.push_back({.Cohort = member.Cell, .FieldSupported = member.FieldSupported});
          }
          const auto &index = (**groups).Index;
          auto selected = SourceProductSelection::Of(
              std::shared_ptr<const PlanHierarchy>(index, &index->Hierarchy), traits, key);
          if (selected) {
            auto cut = selected->Select(eye, projection);
            if (cut) {
              result->Cut = std::move(*cut);
              result->Selection = std::move(*selected);
              result->Groups = std::move(**groups);
            }
          }
        }
        result->Done.store(true, std::memory_order_release);
      })) {
    return;
  }
  Reading = std::move(result);
  ReadingTile = std::move(tile);
}
}
