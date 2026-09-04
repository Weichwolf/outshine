#ifndef OUTSHINE_WORLD_GROUND_COOKEDTILE_H
#define OUTSHINE_WORLD_GROUND_COOKEDTILE_H

#include <cstdint>
#include <span>
#include <vector>

#include "math/Vec3.h"
#include "ClusterDag.h"
#include "TileMeshes.h"

namespace outshine::Ground {

void CookTile(std::span<const StoredVertex> soup,
              int gridverts,
              const Vec3 &origin,
              std::vector<StoredVertex> &outVerts,
              std::vector<uint32_t> &outIdx,
              std::vector<DagCluster> &outClusters);

}
#endif
