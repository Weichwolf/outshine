#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGGEOMETRYSOURCE_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGGEOMETRYSOURCE_H
#include "GeometryAsset.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators {
class PreparedBuildingAssets;

class BuildingGeometrySource {
public:
  struct Recipe {
    std::string BaseKey;
    uint64_t SourceCount = 0;
    std::vector<uint32_t> Sources;
    [[nodiscard]] std::string Key() const;
  };

  struct Budget {
    size_t EncodedBytesMost = 0;
    uint64_t TriangleAttempts = 0;
  };

  // Fine render geometry; caller owns producer/resident admission. EncodedBytesMost is no peak
  // bound.
  [[nodiscard]] static std::expected<GeometryAsset::Product, std::string>
  Resolve(AssetCache &cache,
          PreparedBuildingAssets *sources,
          const Recipe &recipe,
          Budget budget,
          const std::stop_token &stop = {});
};
}
#endif
