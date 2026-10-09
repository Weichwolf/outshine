#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H

#include "GroundRegionAsset.h"
#include "WaterAsset.h"
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
struct TileSpot;
}

namespace outshine::Generators::Osm {
class OsmField;
enum class MvtSchema : uint8_t;

class PreparedGroundRegions {
public:
  static constexpr size_t PackageBytesMost = size_t{512} * 1024u * 1024u;

  struct Loaded {
    GroundRegionAsset Region;
    WaterAsset Water;
    size_t ReadBytes = 0;
    std::string Key;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedGroundRegions>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources, std::string_view rules);
  [[nodiscard]] std::string RequestKey(int zoom,
                                       MvtSchema schema,
                                       const ::outshine::Ground::ShapedGround &shape,
                                       std::span<const uint8_t> parameters,
                                       std::span<const ::outshine::Ground::TileSpot> tiles) const;
  [[nodiscard]] std::string Key(const OsmField &vectors,
                                const ::outshine::Ground::ShapedGround &shape,
                                std::span<const uint8_t> parameters) const;
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string> Load(const std::string &key);
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string>
  LoadRequest(const std::string &requestKey);
  [[nodiscard]] std::expected<void, std::string> BindRequest(const std::string &key,
                                                             const std::string &requestKey);
  [[nodiscard]] std::expected<Loaded, std::string> Store(const std::string &key,
                                                         const Box &bounds,
                                                         const GroundRegionAsset &region,
                                                         const WaterAsset &water,
                                                         const std::string &requestKey = {});

private:
  PreparedGroundRegions(std::unique_ptr<AssetCache> cache, std::string recipe);
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string> Read(const std::string &key);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
};
}
#endif
