#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINASSETS_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINASSETS_H

#include "AssetCache.h"
#include "TerrainTiles.h"
#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators {

class PreparedTerrainAssets {
public:
  struct Counters {
    uint64_t Hits = 0, Misses = 0, Resident = 0, Writes = 0, ReadBytes = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedTerrainAssets>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);

  [[nodiscard]] ::outshine::Ground::TerrainGrid
  Resolve(Data::TileId at,
          const ::outshine::Ground::TerrainTiles::Shaped &shape,
          const ::outshine::Ground::TerrainTiles::FieldFactory &factory);

  [[nodiscard]] Counters Costs() const noexcept;

private:
  PreparedTerrainAssets(std::unique_ptr<AssetCache> cache, std::string recipe);
  [[nodiscard]] ::outshine::Ground::TerrainGrid Remember(const std::string &key,
                                                         ::outshine::Ground::TerrainField field);

  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
  std::unordered_map<std::string, std::weak_ptr<const ::outshine::Ground::TerrainField>> Resident_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, ResidentHits_{0}, Writes_{0}, ReadBytes_{0};
};

}
#endif
