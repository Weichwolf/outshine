#include "CookedTile.h"

#include <string>

#include <Geometry.h>
#include <Material.h>

namespace outshine::Ground {

namespace {

constexpr size_t kTileSoupFloats = 8;

}

void CookTile(const float *soup,
              int nverts,
              int gridverts,
              const double origin[3],
              std::vector<float> &outVerts,
              std::vector<uint32_t> &outIdx,
              std::vector<DagCluster> &outClusters) {
  outVerts.clear();
  outIdx.clear();
  outClusters.clear();
  if (!soup || nverts <= 0) { return; }
  if (gridverts <= 0 || gridverts > nverts) { gridverts = nverts; }

  {
    outVerts.assign(soup, soup + (size_t)gridverts * kTileSoupFloats);
    outIdx.resize((size_t)gridverts);
    for (int vertex = 0; vertex < gridverts; ++vertex) {
      outIdx[(size_t)vertex] = (uint32_t)vertex;
    }
    DagCluster whole{};
    whole.Count = (uint32_t)gridverts;
    whole.ParentErr = kDagRootErr;
    BoundingSphere(
        soup, (uint32_t)gridverts, (int)kTileSoupFloats, whole.SelfCenter, &whole.SelfRadius);
    outClusters.push_back(whole);
  }
}

} // namespace outshine::Ground
