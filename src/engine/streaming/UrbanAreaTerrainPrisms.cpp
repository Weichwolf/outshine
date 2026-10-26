#include "UrbanAreaTerrainPrisms.h"
#include "TerrainResidency.h"
#include "Geodesy.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
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
      const auto height = terrain.HeightMAt({.LongitudeDeg = Wrap180(lot->Centre.LongitudeDeg),
                                             .LatitudeDeg = lot->Centre.LatitudeDeg});
      if (!height || !std::isfinite(*height)) {
        return std::unexpected(UrbanPrismError::MissingTerrain);
      }
      heights[static_cast<size_t>(row) * p.Columns + column] = height;
    }
  }
  return {};
}

std::expected<std::vector<std::optional<double>>, Generators::Osm::UrbanPrismError>
CaptureHeights(const Generators::Osm::UrbanArea &area,
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
  size_t first = 0;
  for (const auto &p : patches) {
    const size_t cells = static_cast<size_t>(p.Columns) * p.Rows;
    const auto read = ReadPatchGround(area, p, terrain, std::span{heights}.subspan(first, cells));
    if (!read) { return std::unexpected(read.error()); }
    first += static_cast<size_t>(p.Columns) * p.Rows;
  }
  return heights;
}

std::vector<Generators::Osm::UrbanPrismInput>
InputsFor(std::span<const Generators::Osm::UrbanPatchRange> patches,
          std::span<const std::optional<double>> heights) {
  std::vector<Generators::Osm::UrbanPrismInput> inputs;
  inputs.reserve(patches.size());
  size_t first = 0;
  for (const auto &p : patches) {
    inputs.push_back({.Range = p,
                      .GroundAslM = Generators::Osm::UrbanGroundHeights{
                          heights.data() + first, p.Rows, p.Columns}});
    first += static_cast<size_t>(p.Columns) * p.Rows;
  }
  return inputs;
}

const char *InputError(Generators::Osm::UrbanPrismError error) {
  using Generators::Osm::UrbanPrismError;
  switch (error) {
    case UrbanPrismError::InvalidRequest: return "invalid selected prism input";
    case UrbanPrismError::MissingTerrain: return "selected prism terrain is not published";
    case UrbanPrismError::OverBudget: return "selected prism input exceeds capture budget";
    case UrbanPrismError::Unrepresentable: return "selected prism input cannot be represented";
  }
  return "unknown selected prism input failure";
}
}

std::expected<Generators::Osm::UrbanPrisms, Generators::Osm::UrbanPrismError>
PrepareUrbanPrisms(const Generators::Osm::UrbanArea &area,
                   std::span<const Generators::Osm::UrbanPatchRange> patches,
                   const TerrainResidency &terrain,
                   size_t samplesMost) {
  const auto heights = CaptureHeights(area, patches, terrain, samplesMost);
  if (!heights) { return std::unexpected(heights.error()); }
  const auto inputs = InputsFor(patches, *heights);
  return Generators::Osm::PrepareUrbanPrisms(area, inputs, samplesMost);
}

std::expected<AssetFactory, std::string>
CaptureUrbanPrismFactory(std::shared_ptr<const Generators::Osm::UrbanArea> area,
                         std::span<const Generators::Osm::UrbanPatchRange> patches,
                         const TerrainResidency &terrain,
                         std::string request,
                         size_t samplesMost,
                         Generators::Osm::UrbanPrismAssetOptions options,
                         const std::stop_token &stop) {
  if (!area || patches.empty() || request.size() != 64 ||
      request.find_first_not_of("0123456789abcdef") != std::string::npos) {
    return std::unexpected("invalid prism capture demand");
  }
  if (stop.stop_requested()) { return std::unexpected("prism input capture canceled"); }
  constexpr size_t placementBytes =
      sizeof(PrismPatch) + sizeof(Generators::Osm::UrbanPrismPlacement);
  if (options.BytesMost == 0 || options.BytesMost > Generators::Osm::kUrbanPrismBytesMost ||
      patches.size() > options.BytesMost / placementBytes) {
    return std::unexpected("selected prism frames exceed byte budget");
  }
  const size_t frameBytes = patches.size() * placementBytes;
  const size_t captureMost =
      std::min(samplesMost, (options.BytesMost - frameBytes) / sizeof(PrismSpan));
  auto heights = CaptureHeights(*area, patches, terrain, captureMost);
  if (!heights) { return std::unexpected(InputError(heights.error())); }
  if (stop.stop_requested()) { return std::unexpected("prism input capture canceled"); }
  return AssetFactory{
      [area = std::move(area),
       ranges = std::vector(patches.begin(), patches.end()),
       heights = std::move(*heights),
       request = std::move(request),
       samplesMost,
       options,
       stop](size_t bytesMost) -> std::expected<GeneratedAssetPackage, std::string> {
        if (stop.stop_requested()) { return std::unexpected("prism input capture canceled"); }
        const size_t budget = std::min(options.BytesMost, bytesMost);
        const size_t frameBytes = ranges.size() * placementBytes;
        if (frameBytes > budget || heights.size() > (budget - frameBytes) / sizeof(PrismSpan)) {
          return std::unexpected("selected prism output exceeds byte budget");
        }
        const auto inputs = InputsFor(ranges, heights);
        auto product = Generators::Osm::PrepareUrbanPrisms(*area, inputs, samplesMost);
        if (!product) { return std::unexpected(InputError(product.error())); }
        if (stop.stop_requested()) { return std::unexpected("prism input capture canceled"); }
        return Generators::Osm::PackageUrbanPrisms(
            *product, request, {.Level = options.Level, .BytesMost = budget});
      }};
}
}
