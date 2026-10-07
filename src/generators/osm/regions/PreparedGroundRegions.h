#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H

#include "GroundRegionAsset.h"
#include "content/AssetCache.h"
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators::Osm {
class PreparedGroundRegions {
public:
  static constexpr size_t PackageBytesMost = size_t{512} * 1024u * 1024u;

  struct Loaded {
    GroundRegionAsset Region;
    size_t ReadBytes = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedGroundRegions>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources, std::string_view rules);
  [[nodiscard]] std::string Key(std::string_view binding) const;
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string> Load(const std::string &key);
  [[nodiscard]] std::expected<Loaded, std::string>
  Store(const std::string &key, const Box &bounds, const GroundRegionAsset &region);

private:
  PreparedGroundRegions(std::unique_ptr<AssetCache> cache, std::string recipe);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
};
}
#endif
