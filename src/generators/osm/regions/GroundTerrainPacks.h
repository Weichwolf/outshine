#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPACKS_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPACKS_H

#include "AssetCache.h"
#include "TerrainPatchwork.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace outshine {
class BinaryValueReader;
class BinaryValueWriter;
}

namespace outshine::Generators::Osm {

struct GroundTerrainManifest {
  struct Pack {
    std::string Key;
    std::vector<uint32_t> Slots;
  };

  uint32_t Sheets = 0;
  std::vector<Pack> Packs;

  [[nodiscard]] bool Write(BinaryValueWriter &out) const;
  [[nodiscard]] bool Read(BinaryValueReader &in);
};

[[nodiscard]] std::expected<GroundTerrainManifest, AssetCacheError>
PrepareGroundTerrainPacks(AssetCache &cache,
                          std::span<const Sheet> sheets,
                          size_t bytesMost,
                          std::vector<PreparedAssetWrite> &into);

class GroundTerrainPackReader {
public:
  [[nodiscard]] std::expected<bool, AssetCacheError> Restore(const AssetCache &cache,
                                                             const AssetRecord &region,
                                                             const GroundTerrainManifest &manifest,
                                                             size_t bytesMost,
                                                             std::vector<Sheet> &sheets,
                                                             size_t &readBytes,
                                                             const std::stop_token &stop);

private:
  using Pages = std::shared_ptr<const std::vector<Sheet>>;

  struct ResidentPack {
    std::weak_ptr<const std::vector<Sheet>> Pages;
    uint64_t NativeBytes = 0;
  };

  [[nodiscard]] std::expected<Pages, AssetCacheError>
  Read(const AssetCache &cache, const AssetRecord &record, size_t count, size_t &readBytes);
  std::mutex Lock_;
  std::unordered_map<std::string, ResidentPack> Resident_;
};
}
#endif
