#include <algorithm>
#include <limits>
#include <expected>
#include "Shape.h"
#include "math/Vec3.h"

#include <chrono>
#include <cstddef>
#include <span>
#include <vector>
#include <cstdint>
#include <ratio>

namespace outshine::Render {

Box Shape::BoundsOf(size_t parts) const {
  const auto fold = [this](size_t upTo) {
    Box over;
    for (size_t part = 0; part < upTo && part < Parts.size(); ++part) {
      const ShapePart &one = Parts[part];
      for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
           ++vertex) {
        over.Cover(Vec3{{static_cast<double>(one.PositionsM[vertex * 3]),
                         static_cast<double>(one.PositionsM[vertex * 3 + 1]),
                         static_cast<double>(one.PositionsM[vertex * 3 + 2])}});
      }
    }
    return over;
  };
  Box whole = fold(Parts.size());
  if (whole.Empty()) { whole = Box{.Min = Vec3{}, .Max = Vec3{}}; }
  if (parts == 0 || parts >= Parts.size()) { return whole; }
  const Box some = fold(parts);
  return some.Empty() ? whole : some;
}

namespace {
std::expected<void, ClusterError> LocalIndices(const ShapePart &part,
                                               std::span<const uint32_t> indices,
                                               std::vector<uint32_t> &local) {
  if (part.FirstIndex > indices.size() || part.IndexCount > indices.size() - part.FirstIndex ||
      part.PositionsM.size() % 3 != 0 || part.VertexCount != part.PositionsM.size() / 3 ||
      part.FirstVertex > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(ClusterError::InvalidLayout);
  }
  local.assign(indices.begin() + static_cast<long>(part.FirstIndex),
               indices.begin() + static_cast<long>(part.FirstIndex + part.IndexCount));
  for (uint32_t &at : local) {
    if (at < part.FirstVertex || at - part.FirstVertex >= part.VertexCount) {
      return std::unexpected(ClusterError::InvalidIndex);
    }
    at -= static_cast<uint32_t>(part.FirstVertex);
  }
  return {};
}
}

std::expected<void, ClusterError> CookShape(ShapeStore &into, std::span<const Material> surfaces) {
  const auto began = std::chrono::steady_clock::now();
  into.Clustering = {};
  into.Clusters.clear();
  into.ClusterSpheres.clear();
  if (into.Indices.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(ClusterError::CapacityExceeded);
  }

  size_t rootless = 0;
  const auto keep = [&into, &rootless](const DagCluster &cut) {
    if (cut.ParentErr >= kDagRootErr) { ++rootless; }
    into.Clusters.push_back(cut);
    into.ClusterSpheres.insert(into.ClusterSpheres.end(),
                               {cut.SelfCenter[0],
                                cut.SelfCenter[1],
                                cut.SelfCenter[2],
                                cut.SelfRadius,
                                cut.ParentCenter[0],
                                cut.ParentCenter[1],
                                cut.ParentCenter[2],
                                cut.ParentRadius,
                                cut.SelfErr,
                                cut.ParentErr,
                                0.0f,
                                0.0f});
  };
  std::vector<uint32_t> local;
  for (ShapePart &part : into.Parts) {
    part.FirstCluster = static_cast<uint32_t>(into.Clusters.size());
    part.ClusterCount = 0;
    const auto rebased = LocalIndices(part, into.Indices, local);
    if (!rebased) { return std::unexpected(rebased.error()); }
    if (part.IndexCount == 0) { continue; }
    const bool cuts =
        part.Material < 0 || static_cast<size_t>(part.Material) >= surfaces.size() ||
        StateOf(surfaces[static_cast<size_t>(part.Material)]).Kind() == SurfaceKind::Opaque ||
        StateOf(surfaces[static_cast<size_t>(part.Material)]).Kind() == SurfaceKind::Masked;
    const auto cooked = CookClusters(
        {.PositionsM = part.PositionsM, .Indices = local},
        cuts ? kClusterTriangles : static_cast<uint32_t>(std::max(size_t{1}, local.size() / 3)));
    if (!cooked) { return std::unexpected(cooked.error()); }
    if (!cuts) { continue; }
    const ClusteredMesh &cut = *cooked;
    for (size_t at = 0; at < cut.Index.size(); ++at) {
      into.Indices[part.FirstIndex + at] = cut.Index[at] + static_cast<uint32_t>(part.FirstVertex);
    }
    for (DagCluster held : cut.Clusters) {
      held.First += static_cast<uint32_t>(part.FirstIndex);
      keep(held);
    }
    part.ClusterCount = static_cast<uint32_t>(cut.Clusters.size());
  }
  into.Clustering = {
      .BuildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                     .count(),
      .RootClusters = rootless,
      .Clusters = into.Clusters.size()};
  return {};
}
}
