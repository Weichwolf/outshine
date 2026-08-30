#include "Shape.h"

namespace outshine::Render {

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


// A PART'S TRIANGLES, CUT AND REORDERED IN PLACE. `CookClusters` sorts a run by the Morton code of
// each triangle's centroid and hands back the reordered run with a sphere and a proven error bound
// per cluster; nothing about the VERTICES moves, so the picture is the same picture and the only
// thing that changed is which triangles sit next to each other.
//
// A BLENDED SURFACE IS LEFT ALONE. Compositing reads the order the triangles arrive in, so
// reordering them is visible -- and Nanite does not take translucency either, which is the same
// answer arrived at for the same reason.
void CookShape(ShapeStore &into, std::span<const Material> surfaces) {
  into.Clusters.clear();
  into.ClusterIndices.clear();
  if (into.Indices.empty()) { return; }
  into.ClusterIndices.reserve(into.Indices.size());
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
    // cluster is culled whole; the sequence inside it is used by no stage. And reordering is not
    // free: where two surfaces COINCIDE this renderer's depth test resolves the tie by which
    // triangle arrived first, so a reorder repaints them -- measured on Khronos's NormalTangentTest,
    // one 198x48 cell whose shading normal flipped its bitangent and whose picture went 6 codes
    // from the oracle to 8, past the case's own bound of 6.435.
    if (part.IndexCount <= (size_t)kClusterTriangles * 3u) {
      DagCluster whole{};
      whole.First = (uint32_t)part.FirstIndex;
      whole.Count = (uint32_t)part.IndexCount;
      whole.ParentErr = kDagRootErr;
      BoundingSphere(part.PositionsM.data(), (uint32_t)(part.PositionsM.size() / 3), 3,
                     whole.SelfCenter, &whole.SelfRadius);
      into.Clusters.push_back(whole);
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
    const uint32_t base = (uint32_t)into.ClusterIndices.size();
    for (const uint32_t at : cut.Index) {
      into.ClusterIndices.push_back(at + (uint32_t)part.FirstVertex);
    }
    for (DagCluster held : cut.Clusters) {
      held.First += base;
      into.Clusters.push_back(held);
    }
    part.ClusterCount = (uint32_t)cut.Clusters.size();
  }
}
}
