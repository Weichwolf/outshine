#include "UrbanAreaTerrainPrisms.h"
#include "TerrainResidency.h"
#include "Geodesy.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
namespace {
std::expected<void, Generators::Osm::UrbanPrismError>
ReadPatchGround(const Generators::Osm::UrbanArea &area,
                Generators::Osm::UrbanPatchRange p,
                const TerrainResidency &terrain,
                std::span<std::optional<double>> heights) {
  using namespace Generators::Osm;
  for (uint32_t row = 0; row < p.Rows; ++row) {
    for (uint32_t column = 0; column < p.Columns; ++column) {
      const size_t source = static_cast<size_t>(p.Row + row) * area.Columns + p.Column + column;
      if (area.Tiers[source] >= UrbanArea::HeightsM.size()) {
        return std::unexpected(UrbanPrismError::InvalidRequest);
      }
      const auto lot = area.LotAt(p.Column + column, p.Row + row);
      if (!lot) { continue; }
      if (!terrain.HasPublishedSurface()) {
        return std::unexpected(UrbanPrismError::MissingTerrain);
      }
      heights[static_cast<size_t>(row) * p.Columns + column] =
          terrain.HeightMAt({.LongitudeDeg = Wrap180(lot->Centre.LongitudeDeg),
                             .LatitudeDeg = lot->Centre.LatitudeDeg});
    }
  }
  return {};
}
}

std::expected<Generators::Osm::UrbanPrisms, Generators::Osm::UrbanPrismError>
PrepareUrbanPrisms(const Generators::Osm::UrbanArea &area,
                   std::span<const Generators::Osm::UrbanPatchRange> patches,
                   const TerrainResidency &terrain,
                   size_t samplesMost) {
  using namespace Generators::Osm;
  if (!area.Recipe.Valid() || area.Tiers.size() != uint64_t{area.Columns} * area.Rows) {
    return std::unexpected(UrbanPrismError::InvalidRequest);
  }
  size_t count = 0;
  for (const auto &p : patches) {
    if (p.Columns == 0 || p.Rows == 0 || p.Column >= area.Columns || p.Row >= area.Rows ||
        p.Columns > area.Columns - p.Column || p.Rows > area.Rows - p.Row ||
        p.Columns > area.Recipe.PatchSide || p.Rows > area.Recipe.PatchSide) {
      return std::unexpected(UrbanPrismError::InvalidRequest);
    }
    const size_t cells = static_cast<size_t>(p.Columns) * p.Rows;
    if (cells > samplesMost - count) { return std::unexpected(UrbanPrismError::OverBudget); }
    count += cells;
  }
  std::vector<std::optional<double>> heights(count);
  std::vector<UrbanPrismInput> inputs;
  inputs.reserve(patches.size());
  size_t first = 0;
  for (const auto &p : patches) {
    const size_t cells = static_cast<size_t>(p.Columns) * p.Rows;
    const auto read = ReadPatchGround(area, p, terrain, std::span{heights}.subspan(first, cells));
    if (!read) { return std::unexpected(read.error()); }
    inputs.push_back(
        {.Range = p, .GroundAslM = UrbanGroundHeights{heights.data() + first, p.Rows, p.Columns}});
    first += static_cast<size_t>(p.Columns) * p.Rows;
  }
  return Generators::Osm::PrepareUrbanPrisms(area, inputs, samplesMost);
}
}
