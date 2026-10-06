#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGASSETS_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGASSETS_H

#include "AssetCache.h"
#include "PreparedStructureTile.h"
#include "TilePool.h"
#include <atomic>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators {
class PreparedBuildingAssets {
public:
  using Base = std::shared_ptr<const PreparedStructureTile>;

  struct Counters {
    uint64_t Hits = 0, Misses = 0, Writes = 0, ReadBytes = 0;
    uint64_t GeometryHits = 0, GeometryMisses = 0, GeometryWrites = 0, GeometryReadBytes = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedBuildingAssets>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);
  [[nodiscard]] std::string Key(Data::TileId tile,
                                uint64_t streetDigest,
                                const ::outshine::Ground::ShapedGround &shape,
                                double spanM,
                                std::string_view inputDigest = {}) const;
  [[nodiscard]] std::expected<Base, StructureBakeError> Load(const std::string &key);
  [[nodiscard]] std::expected<Base, StructureBakeError>
  Generate(const std::string &key,
           const RawTile &raw,
           const ::outshine::Ground::HeightField &heights,
           const std::atomic_bool &stopping);
  [[nodiscard]] std::expected<std::optional<BakedTile>, StructureBakeError>
  LoadGeometry(const std::string &baseKey, const PreparedStructureTile &base, const RawTile &view);
  [[nodiscard]] std::expected<void, StructureBakeError>
  StoreGeometry(const std::string &baseKey,
                const PreparedStructureTile &base,
                const RawTile &view,
                const BakedTile &tile);
  [[nodiscard]] Counters Costs() const noexcept;

private:
  PreparedBuildingAssets(std::unique_ptr<AssetCache> cache, std::string recipe);
  [[nodiscard]] static Box Bounds(const PreparedStructureTile &base);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic_uint64_t GeometryHits_{0}, GeometryMisses_{0}, GeometryWrites_{0},
      GeometryReadBytes_{0};
};
}
#endif
