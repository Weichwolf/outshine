#ifndef OUTSHINE_WORLD_PRODUCTS_CLUSTEREDGEOMETRYCODEC_H
#define OUTSHINE_WORLD_PRODUCTS_CLUSTEREDGEOMETRYCODEC_H
#include "Geometry.h"
#include "ClusterDag.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
using GeometryClusters = std::vector<std::vector<DagCluster>>;

struct ClusteredGeometryProduct {
  Geometry Mesh;
  GeometryClusters Clusters;
};

[[nodiscard]] bool ValidGeometryClusters(const Geometry &geometry,
                                         std::span<const std::vector<DagCluster>> clusters);
[[nodiscard]] std::optional<std::vector<uint8_t>> EncodeClusteredGeometry(
    const Geometry &geometry, std::span<const std::vector<DagCluster>> clusters, size_t bytesMost);
[[nodiscard]] std::optional<ClusteredGeometryProduct>
DecodeClusteredGeometry(std::span<const uint8_t> bytes, size_t bytesMost);
}
#endif
