#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPACKS_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPACKS_H

#include "AssetCache.h"
#include "TerrainPatchwork.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
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

using GroundTerrainErrorLimit = std::function<double(const Box &)>;

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

struct GroundTerrainRead {
  size_t Bytes = 0;
  size_t CompactPacks = 0;
};

class GroundTerrainPackReader {
public:
  [[nodiscard]] std::expected<bool, AssetCacheError> Restore(const AssetCache &cache,
                                                             const AssetRecord &region,
                                                             const GroundTerrainManifest &manifest,
                                                             size_t bytesMost,
                                                             std::vector<Sheet> &sheets,
                                                             GroundTerrainRead &read,
                                                             const std::stop_token &stop,
                                                             const GroundTerrainErrorLimit &limit);

private:
  using Pages = std::shared_ptr<const std::vector<Sheet>>;

  struct ResidentPack {
    std::weak_ptr<const std::vector<Sheet>> Pages;
    std::string Package;
    std::string BasePackage;
    uint64_t NativeBytes = 0;
  };

  [[nodiscard]] Pages Retain(const AssetCache &cache, const AssetRecord &record, size_t count);
  [[nodiscard]] std::expected<Pages, AssetCacheError> Read(const AssetCache &cache,
                                                           const AssetRecord &record,
                                                           const AssetRecord *base,
                                                           size_t count,
                                                           size_t &readBytes);
  [[nodiscard]] std::expected<Pages, AssetCacheError> Materialize(const AssetCache &cache,
                                                                  const AssetRecord &record,
                                                                  const AssetRecord *base,
                                                                  std::span<const uint8_t> bytes,
                                                                  size_t count,
                                                                  size_t &readBytes);
  std::mutex Lock_;
  std::unordered_map<std::string, ResidentPack> Resident_;
};
}
#endif
