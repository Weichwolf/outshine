#include "Shape.h"

#include <atomic>
#include <chrono>

namespace outshine::Render {

std::atomic<double> gCookMs{0.0};
std::atomic<size_t> gRootless{0};
std::atomic<size_t> gClusters{0};

double CookedMs() {
  return gCookMs.load(std::memory_order_relaxed);
}

size_t CookedRootless() {
  return gRootless.load(std::memory_order_relaxed);
}

size_t CookedClusters() {
  return gClusters.load(std::memory_order_relaxed);
}

void Shape::BoundsOf(size_t parts, double leastM[3], double mostM[3]) const {
  const auto fold = [this](size_t upTo, double least[3], double most[3]) {
    bool any = false;
    for (size_t part = 0; part < upTo && part < Parts.size(); ++part) {
      const ShapePart &one = Parts[part];
      for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
           ++vertex) {
        for (int axis = 0; axis < 3; ++axis) {
          const double at = (double)one.PositionsM[vertex * 3 + (size_t)axis];
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
  double least[3], most[3];
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
    part.FirstCluster = (uint32_t)into.Clusters.size();
    part.ClusterCount = 0;
    if (part.IndexCount < 3 || part.PositionsM.size() < 3) { continue; }
    const bool cuts = part.Material < 0 || (size_t)part.Material >= surfaces.size() ||
                      StateOf(surfaces[(size_t)part.Material]).Kind() == SurfaceKind::Opaque ||
                      StateOf(surfaces[(size_t)part.Material]).Kind() == SurfaceKind::Masked;
    if (!cuts) { continue; }

    if (part.IndexCount <= (size_t)kClusterTriangles * 3u) {
      DagCluster whole{};
      whole.First = (uint32_t)part.FirstIndex;
      whole.Count = (uint32_t)part.IndexCount;
      whole.ParentErr = kDagRootErr;
      BoundingSphere(part.PositionsM.data(),
                     (uint32_t)(part.PositionsM.size() / 3),
                     3,
                     whole.SelfCenter,
                     &whole.SelfRadius);
      keep(whole);
      part.ClusterCount = 1;
      continue;
    }

    local.assign(into.Indices.begin() + (long)part.FirstIndex,
                 into.Indices.begin() + (long)(part.FirstIndex + part.IndexCount));
    for (uint32_t &at : local) { at -= (uint32_t)part.FirstVertex; }
    const Cooked cut = CookClusters(part.PositionsM, local, kClusterTriangles);
    if (cut.Index.size() != local.size() || cut.Clusters.empty()) { continue; }
    for (size_t at = 0; at < cut.Index.size(); ++at) {
      into.Indices[part.FirstIndex + at] = cut.Index[at] + (uint32_t)part.FirstVertex;
    }
    for (DagCluster held : cut.Clusters) {
      held.First += (uint32_t)part.FirstIndex;
      keep(held);
    }
    part.ClusterCount = (uint32_t)cut.Clusters.size();
  }
  gRootless.store(rootless, std::memory_order_relaxed);
  gClusters.store(into.Clusters.size(), std::memory_order_relaxed);
  gCookMs.store(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      std::memory_order_relaxed);
}
} // namespace outshine::Render
