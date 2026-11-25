#ifndef OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDTERRACEASSET_H
#define OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDTERRACEASSET_H

#include "AssetPackage.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine::Generators {
class PreparedBuildingAssets;

namespace Osm {
struct TerraceAssetRequest {
  std::string BaseKey;
  unsigned SpacingM = 0;
  uint64_t Cells = ~uint64_t{0};

  [[nodiscard]] std::expected<std::string, std::string> Key() const;
};

struct TerraceAssetBudget {
  size_t SamplesMost = 0;
  size_t PointTestsMost = 0;
};

[[nodiscard]] std::expected<AssetPackageFactory, std::string>
CapturePreparedTerraces(std::shared_ptr<PreparedBuildingAssets> assets,
                        const TerraceAssetRequest &request,
                        TerraceAssetBudget budget,
                        const std::stop_token &stop);
}
}
#endif
