#ifndef OUTSHINE_CONTENT_ASSETS_GEOMETRYASSET_H
#define OUTSHINE_CONTENT_ASSETS_GEOMETRYASSET_H
#include "AssetCache.h"
#include "Geometry.h"
#include "Mat4.h"
#include "SourceCoverage.h"
#include "ClusterDag.h"
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {
class GeometryAsset {
public:
  struct Description {
    Box Bounds;
    Mat4 WorldFromLocal;
    std::optional<SourceCoverage> Coverage = std::nullopt;
    uint32_t ClusteredParts = 0;
  };

  struct Product {
    Description Layout;
    Geometry Mesh;
    std::vector<std::vector<DagCluster>> Clusters;
    [[nodiscard]] size_t HeapBytes() const noexcept;
  };

  [[nodiscard]] static std::expected<std::optional<Description>, AssetCacheError> Describe(
      const AssetCache &cache, std::string_view key, std::string_view parent, size_t bytesMost);
  [[nodiscard]] static std::expected<std::optional<Product>, AssetCacheError>
  Load(const AssetCache &cache, std::string_view key, std::string_view parent, size_t bytesMost);
  // bytesMost bounds each encoded mesh/description, not workspace or total producer residency.
  [[nodiscard]] static std::expected<void, AssetCacheError>
  Publish(AssetCache &cache,
          const std::string &key,
          const std::string &parent,
          const Description &description,
          const Geometry &geometry,
          size_t bytesMost,
          std::span<const std::vector<DagCluster>> clusters = {});

  [[nodiscard]] static std::expected<void, AssetCacheError> Publish(AssetCache &cache,
                                                                    const std::string &key,
                                                                    const std::string &parent,
                                                                    const Product &product,
                                                                    size_t bytesMost);
};
}
#endif
