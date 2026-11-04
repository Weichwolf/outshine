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
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
bool BuildingFieldStreaming::State::PrepareCellSources(
    Tile &tile, const Generators::PreparedBuildingAssets::SourceGroups &groups) {
  std::vector<std::vector<uint32_t>> members(tile.Source.CellSourceCounts.size());
  for (size_t at = 0; at < groups.Members.size(); ++at) {
    const auto cell = groups.Members[at].Cell;
    if (cell == 0 || cell > members.size()) { return false; }
    members[cell - 1u].push_back(static_cast<uint32_t>(at));
  }
  tile.Cells.reserve(groups.Cells.size());
  for (const auto &source : groups.Cells) {
    if (source.Cell == 0 || source.Cell > members.size() ||
        members[source.Cell - 1u].size() != tile.Source.CellSourceCounts[source.Cell - 1u]) {
      return false;
    }
    auto coverage = SourceCoverage::FromSources(
        tile.Source.BaseKey, groups.Members.size(), members[source.Cell - 1u]);
    if (!coverage) { return false; }
    tile.Cells.emplace_back(source, std::move(*coverage));
  }
  std::vector<SourceCoverage::Range> ranges;
  for (const auto &cell : tile.Cells) {
    const auto source = cell.Coverage.Ranges();
    ranges.insert(ranges.end(), source.begin(), source.end());
  }
  std::ranges::sort(ranges, {}, &SourceCoverage::Range::First);
  auto coverage = SourceCoverage::FromRanges(tile.Source.BaseKey, groups.Members.size(), ranges);
  if (!coverage) { return false; }
  tile.Coverage = std::move(*coverage);
  return true;
}

bool BuildingFieldStreaming::State::PrepareCells(
    Tile &tile,
    const Generators::PreparedBuildingAssets::SourceGroups &groups,
    const SourceProductSelection &selection,
    const SourceProductSelection::Cut &cut) {
  if (!PrepareCellSources(tile, groups)) { return false; }
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
    std::ranges::sort(group.Sources);
    auto coverage =
        SourceCoverage::FromSources(tile.Source.BaseKey, groups.Members.size(), group.Sources);
    if (!coverage) { return false; }
    group.Coverage = std::move(*coverage);
    group.Bounds = selection.Bounds(product);
    cell->Groups.push_back(std::move(group));
  }
  for (auto &cell : tile.Cells) {
    std::vector<const SourceCoverage *> products;
    products.reserve(cell.Groups.size());
    for (const auto &group : cell.Groups) {
      products.push_back(group.Coverage ? &*group.Coverage : nullptr);
    }
    if (cell.Where != Phase::Rejected && !cell.Groups.empty() &&
        SourceCoverage::Partition(cell.Coverage, products)) {
      cell.Alternatives = std::move(cell.Groups);
    }
    cell.Groups.clear();
    Group coarse;
    coarse.Bounds = cell.Source.Bounds;
    coarse.Coverage = cell.Coverage;
    cell.Groups.push_back(std::move(coarse));
    cell.Where = Phase::Waiting;
  }
  return true;
}

void BuildingFieldStreaming::State::PollMetadata() {
  if (!Reading || !Reading->Done.load(std::memory_order_acquire)) { return; }
  if (Reading->Groups && Reading->Selection && Reading->Cut && ReadingTile) {
    if (PrepareCells(*ReadingTile, *Reading->Groups, *Reading->Selection, *Reading->Cut)) {
      Tiles.push_back(std::move(*ReadingTile));
    }
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
            .Cells = {},
            .Coverage = std::nullopt,
            .MainProducts = {},
            .ShadowProducts = {},
            .CandidateProducts = {}};
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
            traits.push_back({.Cohort = member.Cell,
                              .FieldSupported = member.FieldSupported,
                              .Faces = member.Faces,
                              .ShellErrorM = member.ShellErrorM});
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
