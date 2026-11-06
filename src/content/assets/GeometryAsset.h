#ifndef OUTSHINE_CONTENT_ASSETS_GEOMETRYASSET_H
#define OUTSHINE_CONTENT_ASSETS_GEOMETRYASSET_H
#include "AssetCache.h"
#include "Geometry.h"
#include "Mat4.h"
#include "SourceCoverage.h"
#include "ClusterDag.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {
class GeometryAsset {
public:
  struct ContentMetrics {
    uint64_t Vertices = 0;
    uint64_t Indices = 0;
    uint64_t AttributeBytes = 0;
    uint64_t ImageBytes = 0;
    uint64_t Clusters = 0;

    // Logical buffers exclude container overhead, workspace and GPU allocations.
    [[nodiscard]] std::optional<uint64_t> BufferBytes() const noexcept;
    [[nodiscard]] bool operator==(const ContentMetrics &) const = default;
  };

  struct Description {
    Box Bounds;
    Mat4 WorldFromLocal;
    std::optional<SourceCoverage> Coverage = std::nullopt;
    uint32_t ClusteredParts = 0;
    std::optional<ContentMetrics> Metrics = std::nullopt;
  };

  struct Product {
    Description Layout;
    Geometry Mesh;
    std::vector<std::vector<DagCluster>> Clusters;
    [[nodiscard]] size_t HeapBytes() const noexcept;
  };

  [[nodiscard]] static std::expected<std::optional<Description>, AssetCacheError> Describe(
      const AssetCache &cache, std::string_view key, std::string_view parent, size_t bytesMost);
  // Measured buffers reject before payload IO; legacy costs are checked after decoding.
  [[nodiscard]] static std::expected<std::optional<Product>, AssetCacheError>
  Load(const AssetCache &cache,
       std::string_view key,
       std::string_view parent,
       size_t bytesMost,
       std::optional<uint64_t> bufferBytesMost = std::nullopt);
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

private:
  [[nodiscard]] static ContentMetrics
  MeasureContent(const Geometry &geometry, std::span<const std::vector<DagCluster>> clusters);
};
}
#endif
