#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPACKS_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_GROUNDTERRAINPACKS_H

#include "AssetCache.h"
#include "TerrainPatchwork.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
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
                          std::string_view revision,
                          std::span<const Sheet> sheets,
                          size_t bytesMost,
                          std::vector<PreparedAssetWrite> &into);

[[nodiscard]] std::expected<bool, AssetCacheError>
RestoreGroundTerrainPacks(const AssetCache &cache,
                          const AssetRecord &region,
                          const GroundTerrainManifest &manifest,
                          size_t bytesMost,
                          std::vector<Sheet> &sheets,
                          size_t &readBytes,
                          const std::stop_token &stop);
}
#endif
