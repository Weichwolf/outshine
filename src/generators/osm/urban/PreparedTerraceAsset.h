#ifndef OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDTERRACEASSET_H
#define OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDTERRACEASSET_H

#include "AssetPackage.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine {
class AssetCache;
}

namespace outshine::Generators {
class PreparedBuildingAssets;
struct PreparedStructureTile;

namespace Osm {
struct TerraceAssetRequest {
  std::string BaseKey;
  unsigned SpacingM = 0;
  uint64_t Cells = ~uint64_t{0};
  unsigned SupportMostM = 0;
  bool RetainRemainder = false;

  [[nodiscard]] std::expected<std::string, std::string> Key() const;
};

struct TerraceRemainder {
  uint32_t SourceCount = 0;
  std::vector<uint32_t> Sources;
};

struct TerraceAssetBudget {
  size_t SamplesMost = 0;
  size_t PointTestsMost = 0;
};

[[nodiscard]] std::expected<TerraceRemainder, std::string> ReadPreparedTerraceRemainder(
    const AssetCache &cache, const TerraceAssetRequest &request, size_t bytesMost);

[[nodiscard]] std::expected<AssetPackage, std::string>
PackagePreparedTerraces(const PreparedStructureTile &source,
                        const TerraceAssetRequest &request,
                        TerraceAssetBudget budget,
                        size_t bytesMost,
                        const std::stop_token &stop);

[[nodiscard]] std::expected<AssetPackageFactory, std::string>
CapturePreparedTerraces(std::shared_ptr<PreparedBuildingAssets> assets,
                        const TerraceAssetRequest &request,
                        TerraceAssetBudget budget,
                        const std::stop_token &stop);
}
}
#endif
