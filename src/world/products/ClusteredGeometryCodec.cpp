#include "ClusteredGeometryCodec.h"
#include "ClusterCook.h"
#include "GeometryProductCodec.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr uint64_t kFormat = 0x31444c434f4547ULL;
constexpr size_t kClusterBytes = 10 * sizeof(float) + 3 * sizeof(uint32_t);

template <class Archive, class Cluster> bool Transfer(Archive &io, Cluster &cluster) {
  return io(cluster.SelfCenter.Axis,
            cluster.SelfRadius,
            cluster.ParentCenter.Axis,
            cluster.ParentRadius,
            cluster.SelfErr,
            cluster.ParentErr,
            cluster.First,
            cluster.Count,
            cluster.Level);
}

}

bool ValidGeometryClusters(const Geometry &geometry,
                           std::span<const std::vector<DagCluster>> clusters) {
  if (geometry.parts() < 0 || clusters.size() != static_cast<size_t>(geometry.parts())) {
    return false;
  }
  for (size_t part = 0; part < clusters.size(); ++part) {
    const size_t indices = geometry.trianglesOf(static_cast<int>(part)).size();
    if (!ValidClusterRanges(clusters[part], indices)) { return false; }
  }
  return true;
}

std::optional<std::vector<uint8_t>> EncodeClusteredGeometry(
    const Geometry &geometry, std::span<const std::vector<DagCluster>> clusters, size_t bytesMost) {
  if (!ValidGeometryClusters(geometry, clusters)) { return std::nullopt; }
  auto mesh = EncodeGeometryProduct(geometry, bytesMost);
  if (!mesh) { return std::nullopt; }
  BinaryValueWriter out(bytesMost);
  if (!out(kFormat, uint64_t{mesh->size()}) || !out.Out.Put(*mesh) ||
      !out(static_cast<uint32_t>(clusters.size()))) {
    return std::nullopt;
  }
  for (const auto &part : clusters) {
    if (part.size() > std::numeric_limits<uint32_t>::max() ||
        !out(static_cast<uint32_t>(part.size()))) {
      return std::nullopt;
    }
    for (const auto &cluster : part) {
      if (!Transfer(out, cluster)) { return std::nullopt; }
    }
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<ClusteredGeometryProduct> DecodeClusteredGeometry(std::span<const uint8_t> bytes,
                                                                size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::nullopt; }
  BinaryValueReader in(bytes);
  uint64_t format = 0;
  uint64_t meshBytes = 0;
  if (!in(format, meshBytes) || format != kFormat || meshBytes > in.In.Remaining()) {
    return std::nullopt;
  }
  const auto encoded = in.In.Take(static_cast<size_t>(meshBytes));
  if (!encoded) { return std::nullopt; }
  auto mesh = DecodeGeometryProduct(*encoded, bytesMost);
  uint32_t parts = 0;
  if (!mesh || !in(parts) || !std::cmp_equal(parts, mesh->parts()) ||
      parts > in.In.Remaining() / sizeof(uint32_t)) {
    return std::nullopt;
  }
  GeometryClusters clusters(parts);
  for (auto &part : clusters) {
    uint32_t count = 0;
    if (!in(count) || count > in.In.Remaining() / kClusterBytes) { return std::nullopt; }
    part.resize(count);
    for (auto &cluster : part) {
      if (!Transfer(in, cluster)) { return std::nullopt; }
    }
  }
  if (in.In.Remaining() != 0 || !ValidGeometryClusters(*mesh, clusters)) { return std::nullopt; }
  return ClusteredGeometryProduct{.Mesh = std::move(*mesh), .Clusters = std::move(clusters)};
}
}
