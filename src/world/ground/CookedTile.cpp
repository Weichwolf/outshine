#include "CookedTile.h"

#include <span>

#include "spatial/ClusterCook.h"
#include "math/Vec3.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include <scene/Geometry.h>
#include <scene/Material.h>
#include <vector>

namespace outshine::Ground {

namespace {

constexpr size_t kTileSoupFloats = 8;

constexpr uint32_t kTileClusterTriangles = 128;

} // namespace

void CookTile(const float *soup,
              int nverts,
              int gridverts,
              [[maybe_unused]] const Vec3 &origin,
              std::vector<float> &outVerts,
              std::vector<uint32_t> &outIdx,
              std::vector<DagCluster> &outClusters) {
  outVerts.clear();
  outIdx.clear();
  outClusters.clear();
  if ((soup == nullptr) || nverts <= 0) { return; }
  if (gridverts <= 0 || gridverts > nverts) { gridverts = nverts; }

  outVerts.assign(soup, soup + static_cast<size_t>(gridverts) * kTileSoupFloats);
  outIdx.resize(static_cast<size_t>(gridverts));
  for (int vertex = 0; vertex < gridverts; ++vertex) {
    outIdx[static_cast<size_t>(vertex)] = static_cast<uint32_t>(vertex);
  }

  const Cooked cut =
      CookClusters(std::span<const float>(soup, static_cast<size_t>(gridverts) * kTileSoupFloats),
                   outIdx,
                   kTileClusterTriangles,
                   static_cast<int>(kTileSoupFloats));
  if (cut.Clusters.empty() || cut.Index.size() != outIdx.size()) {
    DagCluster whole{};
    whole.Count = static_cast<uint32_t>(gridverts);
    whole.ParentErr = kDagRootErr;
    const Bounding around = BoundingSphere({.Floats = soup,
                                            .Count = static_cast<uint32_t>(gridverts),
                                            .Stride = static_cast<int>(kTileSoupFloats)});
    whole.SelfCenter = around.CentreM;
    whole.SelfRadius = around.RadiusM;
    outClusters.push_back(whole);
    return;
  }
  outIdx = cut.Index;
  outClusters = cut.Clusters;
}

} // namespace outshine::Ground
