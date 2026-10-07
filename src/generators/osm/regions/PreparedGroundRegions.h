#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H

#include "GroundRegionAsset.h"
#include "WaterField.h"
#include "content/AssetCache.h"
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Ground {
struct ShapedGround;
}

namespace outshine::Generators::Osm {
class OsmField;

class PreparedGroundRegions {
public:
  static constexpr size_t PackageBytesMost = size_t{512} * 1024u * 1024u;

  struct Loaded {
    GroundRegionAsset Region;
    WaterField Water;
    size_t ReadBytes = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedGroundRegions>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources, std::string_view rules);
  [[nodiscard]] std::string Key(const OsmField &vectors,
                                const ::outshine::Ground::ShapedGround &shape,
                                std::span<const uint8_t> parameters) const;
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string> Load(const std::string &key,
                                                                       const OsmField &source);
  [[nodiscard]] std::expected<Loaded, std::string> Store(const std::string &key,
                                                         const Box &bounds,
                                                         const GroundRegionAsset &region,
                                                         const OsmField &source,
                                                         const WaterField &water);

private:
  PreparedGroundRegions(std::unique_ptr<AssetCache> cache, std::string recipe);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
};
}
#endif
