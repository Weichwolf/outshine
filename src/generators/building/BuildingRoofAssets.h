#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGROOFASSETS_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGROOFASSETS_H

#include "AssetCache.h"
#include "BuildingRoofProxy.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators {
class PreparedBuildingAssets;

class BuildingRoofAssets {
public:
  struct Block {
    double MaximumDiameterM = 0;
    uint32_t Rows = 0;
  };

  [[nodiscard]] static std::expected<std::optional<BuildingRoofAssets>, AssetCacheError>
  Open(const AssetCache &cache, std::string_view baseKey);

  [[nodiscard]] static std::expected<BuildingRoofAssets, AssetCacheError>
  Resolve(AssetCache &cache,
          PreparedBuildingAssets *inputs,
          const std::string &baseKey,
          const std::stop_token &stop = {});

  [[nodiscard]] size_t Prefix(double diameterM) const noexcept;

  [[nodiscard]] const Vec3 &AnchorEcef() const noexcept { return AnchorEcef_; }

  [[nodiscard]] const Box &Bounds() const noexcept { return Bounds_; }

  [[nodiscard]] uint32_t SourceCount() const noexcept { return SourceCount_; }

  [[nodiscard]] std::span<const Block> Blocks() const noexcept { return Blocks_; }

  [[nodiscard]] std::expected<std::optional<std::vector<BuildingRoofProxy>>, AssetCacheError>
  LoadBlock(const AssetCache &cache, size_t index) const;

private:
  static constexpr uint32_t kRowsPerBlock = 128;
  std::string Key_;
  Vec3 AnchorEcef_;
  Box Bounds_;
  uint32_t SourceCount_ = 0;
  std::vector<Block> Blocks_;
};
}
#endif
