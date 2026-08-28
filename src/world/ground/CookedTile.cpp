#include "CookedTile.h"

#include <string>

#include <Geometry.h>
#include <Material.h>

#include "Cooked.h"

namespace outshine::Ground {

namespace {

constexpr size_t kTileSoupFloats = 8;


}

void CookTile(const float *soup, int nverts, int gridverts, const double origin[3],
              std::vector<float> &outVerts, std::vector<uint32_t> &outIdx,
              std::vector<DagCluster> &outClusters) {
  outVerts.clear();
  outIdx.clear();
  outClusters.clear();
  if (!soup || nverts <= 0) { return; }
  if (gridverts <= 0 || gridverts > nverts) { gridverts = nverts; }

  // A TERRAIN TILE NEEDS NO CLUSTER DAG, BECAUSE THE PYRAMID IS ALREADY THE LOD. Zoom z-1 IS the
  // simplified version of zoom z, produced once by whoever made the tiles, so building a
  // Nanite-style DAG inside each tile at runtime rebuilds a reduction the data already carries.
  // Nanite builds its DAG OFFLINE at import; Cesium ships quantized-mesh tiles pre-simplified per
  // level and cuts the quadtree by screen-space error. Neither simplifies terrain at runtime.
  // Measured, and this is why it is here rather than an opinion: sampling a place mid-load put 572
  // of the stack samples in `dag::Clustered`, `dag::SimplifyGroup` and `dag::Absorb` -- the whole
  // CPU cost of standing a world, spent reducing 2 048 triangles per tile down to 8 across every
  // one of 128 tiles. The DAG belongs to SUBJECTS, which have no natural pyramid; that is exactly
  // where Nanite uses it.
  {
    outVerts.assign(soup, soup + (size_t)gridverts * kTileSoupFloats);
    outIdx.resize((size_t)gridverts);
    for (int vertex = 0; vertex < gridverts; ++vertex) { outIdx[(size_t)vertex] = (uint32_t)vertex; }
    DagCluster whole{};
    whole.Count = (uint32_t)gridverts;
    whole.ParentErr = kDagRootErr;
    BoundingSphere(soup, (uint32_t)gridverts, (int)kTileSoupFloats, whole.SelfCenter,
                   &whole.SelfRadius);
    outClusters.push_back(whole);
  }

  const int skirt = nverts - gridverts;
  if (skirt <= 0) { return; }
  const uint32_t base = (uint32_t)(outVerts.size() / kTileSoupFloats);
  DagCluster hem{};
  hem.First = (uint32_t)outIdx.size();
  hem.Count = (uint32_t)skirt;
  hem.SelfErr = 0.0f;
  hem.ParentErr = kDagRootErr;
  double centre[3] = {0, 0, 0};
  for (int at = 0; at < skirt; ++at) {
    for (int axis = 0; axis < 3; ++axis) {
      centre[axis] += soup[(size_t)(gridverts + at) * kTileSoupFloats + (size_t)axis];
    }
  }
  for (int axis = 0; axis < 3; ++axis) { hem.SelfCenter[axis] = (float)(centre[axis] / skirt); }
  double furthest = 0.0;
  for (int at = 0; at < skirt; ++at) {
    double away = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double step =
          soup[(size_t)(gridverts + at) * kTileSoupFloats + (size_t)axis] - (double)hem.SelfCenter[axis];
      away += step * step;
    }
    furthest = furthest > away ? furthest : away;
  }
  hem.SelfRadius = (float)std::sqrt(furthest);
  outVerts.insert(outVerts.end(), soup + (size_t)gridverts * kTileSoupFloats,
                  soup + (size_t)nverts * kTileSoupFloats);
  for (int at = 0; at < skirt; ++at) { outIdx.push_back(base + (uint32_t)at); }
  outClusters.push_back(hem);
}

}
