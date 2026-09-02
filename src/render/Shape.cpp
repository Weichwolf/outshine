#include "Shape.h"
#include "math/Vec3.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <span>
#include <vector>
#include <cstdint>
#include <ratio>

namespace outshine::Render {

namespace {

std::atomic<double> gCookMs{0.0};
std::atomic<size_t> gRootless{0};
std::atomic<size_t> gClusters{0};

} // namespace

double CookedMs() {
  return gCookMs.load(std::memory_order_relaxed);
}

size_t CookedRootless() {
  return gRootless.load(std::memory_order_relaxed);
}

size_t CookedClusters() {
  return gClusters.load(std::memory_order_relaxed);
}

void Shape::BoundsOf(size_t parts, Vec3 &leastM, Vec3 &mostM) const {
  const auto fold = [this](size_t upTo, Vec3 &least, Vec3 &most) {
    bool any = false;
    for (size_t part = 0; part < upTo && part < Parts.size(); ++part) {
      const ShapePart &one = Parts[part];
      for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
           ++vertex) {
        for (int axis = 0; axis < 3; ++axis) {
          const auto at =
              static_cast<double>(one.PositionsM[vertex * 3 + static_cast<size_t>(axis)]);
          if (!any || at < least[axis]) { least[axis] = at; }
          if (!any || at > most[axis]) { most[axis] = at; }
        }
        any = true;
      }
    }
    return any;
  };
  for (int axis = 0; axis < 3; ++axis) {
    leastM[axis] = 0.0;
    mostM[axis] = 0.0;
  }
  (void)fold(Parts.size(), leastM, mostM);
  if (parts == 0 || parts >= Parts.size()) { return; }
  Vec3 least;
  Vec3 most;
  if (!fold(parts, least, most)) { return; }
  for (int axis = 0; axis < 3; ++axis) {
    leastM[axis] = least[axis];
    mostM[axis] = most[axis];
  }
}

void CookShape(ShapeStore &into, std::span<const Material> surfaces) {
  const auto began = std::chrono::steady_clock::now();
  gCookMs.store(0.0, std::memory_order_relaxed);
  gRootless.store(0u, std::memory_order_relaxed);
  into.Clusters.clear();
  into.ClusterSpheres.clear();
  if (into.Indices.empty()) { return; }

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
    if (part.IndexCount < 3 || part.PositionsM.size() < 3) { continue; }
    const bool cuts =
        part.Material < 0 || static_cast<size_t>(part.Material) >= surfaces.size() ||
        StateOf(surfaces[static_cast<size_t>(part.Material)]).Kind() == SurfaceKind::Opaque ||
        StateOf(surfaces[static_cast<size_t>(part.Material)]).Kind() == SurfaceKind::Masked;
    if (!cuts) { continue; }

    if (part.IndexCount <= static_cast<size_t>(kClusterTriangles) * 3u) {
      DagCluster whole{};
      whole.First = static_cast<uint32_t>(part.FirstIndex);
      whole.Count = static_cast<uint32_t>(part.IndexCount);
      whole.ParentErr = kDagRootErr;
      const Bounding around = BoundingSphere(
          part.PositionsM.data(), static_cast<uint32_t>(part.PositionsM.size() / 3), 3);
      whole.SelfCenter = around.CentreM;
      whole.SelfRadius = around.RadiusM;
      keep(whole);
      part.ClusterCount = 1;
      continue;
    }

    local.assign(into.Indices.begin() + static_cast<long>(part.FirstIndex),
                 into.Indices.begin() + static_cast<long>(part.FirstIndex + part.IndexCount));
    for (uint32_t &at : local) { at -= static_cast<uint32_t>(part.FirstVertex); }
    const Cooked cut = CookClusters(part.PositionsM, local, kClusterTriangles);
    if (cut.Index.size() != local.size() || cut.Clusters.empty()) { continue; }
    for (size_t at = 0; at < cut.Index.size(); ++at) {
      into.Indices[part.FirstIndex + at] = cut.Index[at] + static_cast<uint32_t>(part.FirstVertex);
    }
    for (DagCluster held : cut.Clusters) {
      held.First += static_cast<uint32_t>(part.FirstIndex);
      keep(held);
    }
    part.ClusterCount = static_cast<uint32_t>(cut.Clusters.size());
  }
  gRootless.store(rootless, std::memory_order_relaxed);
  gClusters.store(into.Clusters.size(), std::memory_order_relaxed);
  gCookMs.store(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      std::memory_order_relaxed);
}
} // namespace outshine::Render
