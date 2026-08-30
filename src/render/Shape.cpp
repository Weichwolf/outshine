#include "Shape.h"

#include <atomic>
#include <chrono>

namespace outshine::Render {

std::atomic<double> gCookMs{0.0};

double CookedMs() {
  return gCookMs.load(std::memory_order_relaxed);
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

// A PART'S TRIANGLES, CUT AND REORDERED IN THE RUN THAT IS DRAWN. `CookClusters` sorts a run by the
// Morton code of each triangle's centroid and hands back the reordered run with a sphere and a
// proven error bound per cluster; nothing about the VERTICES moves, so the same triangles are drawn
// and the only thing that changed is which of them sit next to each other.
//
// IN PLACE, AND THAT IS A DECISION WITH A MEASURED PRICE. A SECOND run was kept here for one round
// -- the cooked order beside the given one -- because where two surfaces COINCIDE this renderer's
// depth test resolves the tie by which triangle arrived first, and Khronos's NormalTangentTest has
// one 198x48 cell that goes from 6 codes off the oracle to 8 when the order changes, past its own
// bound of 6.435. The second run cost Shibuya 113 MB on this side, 113 MB on the device and 113 MB
// of staging, and the run it was protecting is the one a culled part no longer draws from: a batch
// the culler decides reads the COMPACTED run, which is in cluster order regardless. So the second
// run bought a tie-break for parts that are cut and drawn directly, and was paid for by every part
// that is cut at all. It goes, and the corpus is what says whether the tie-break is missed.
//
// A BLENDED SURFACE IS LEFT ALONE. Compositing reads the order the triangles arrive in, so
// reordering them is visible -- and Nanite does not take translucency either, which is the same
// answer arrived at for the same reason.
void CookShape(ShapeStore &into, std::span<const Material> surfaces) {
  const auto began = std::chrono::steady_clock::now();
  gCookMs.store(0.0, std::memory_order_relaxed);
  into.Clusters.clear();
  into.ClusterSpheres.clear();
  if (into.Indices.empty()) { return; }

  // ONE APPEND WRITES BOTH, so the cooker's record and the device's run cannot disagree about how
  // many clusters there are or where one starts.
  const auto keep = [&into](const DagCluster &cut) {
    into.Clusters.push_back(cut);
    into.ClusterSpheres.insert(
        into.ClusterSpheres.end(),
        {cut.SelfCenter[0], cut.SelfCenter[1], cut.SelfCenter[2], cut.SelfRadius});
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

    // A PART THAT FITS IN ONE CLUSTER IS NOT REORDERED, because nothing would read the order. A
    // cluster is culled whole and the sequence inside it is used by no stage, so the cheapest
    // correct answer is to leave the run exactly as it arrived.
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

    // THE CUT IS PER PART AND SO ARE ITS INDICES. A part's positions are its OWN span -- a world
    // shape views the producer's arrays and joins nothing but the indices -- so the run is made
    // part-local for the cooker and put back global afterwards. A cluster belongs to one part
    // because a surface does.
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
  gCookMs.store(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      std::memory_order_relaxed);
}
} // namespace outshine::Render
