#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINASSETS_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINASSETS_H

#include "content/AssetCache.h"
#include "TerrainTiles.h"
#include "PreparedTerrainDeformation.h"
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

class GroundPatch;

class PreparedTerrainAssets {
public:
  struct Counters {
    uint64_t Hits = 0, Misses = 0, Resident = 0, Writes = 0, ReadBytes = 0;
    uint64_t DeformationHits = 0, DeformationMisses = 0, DeformationWrites = 0,
             DeformationReadBytes = 0;
    uint64_t PatchHits = 0, PatchMisses = 0, PatchWrites = 0, PatchReadBytes = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedTerrainAssets>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);

  [[nodiscard]] ::outshine::Ground::TerrainGrid
  Resolve(Data::TileId at,
          const ::outshine::Ground::TerrainTiles::Shaped &shape,
          const ::outshine::Ground::TerrainTiles::FieldFactory &factory);

  [[nodiscard]] std::expected<std::optional<PreparedTerrainDeformation>, std::string>
  LoadDeformation(const std::string &key, size_t heapBytesMost = kTerrainDeformationBytesMost);
  [[nodiscard]] std::expected<void, std::string>
  StoreDeformation(const std::string &key, const PreparedTerrainDeformation &product);

  [[nodiscard]] std::string PatchKey(Data::TileId at,
                                     const ::outshine::Ground::TerrainTiles::Shaped &shape,
                                     int side,
                                     int blockZoom) const;
  [[nodiscard]] std::expected<std::shared_ptr<const GroundPatch>, std::string>
  LoadPatch(const std::string &key, Data::TileId at, int side);
  [[nodiscard]] std::expected<void, std::string>
  StorePatch(const std::string &key, Data::TileId at, const GroundPatch &patch);

  [[nodiscard]] Counters Costs() const noexcept;

private:
  PreparedTerrainAssets(std::unique_ptr<AssetCache> cache, std::string recipe);
  [[nodiscard]] ::outshine::Ground::TerrainGrid Remember(const std::string &key,
                                                         ::outshine::Ground::TerrainField field);

  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
  std::unordered_map<std::string, std::weak_ptr<const ::outshine::Ground::TerrainField>> Resident_;
  std::atomic_uint64_t DeformationHits_{0}, DeformationMisses_{0}, DeformationWrites_{0},
      DeformationReadBytes_{0};
  std::atomic_uint64_t Hits_{0}, Misses_{0}, ResidentHits_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic_uint64_t PatchHits_{0}, PatchMisses_{0}, PatchWrites_{0}, PatchReadBytes_{0};
};

}
#endif
