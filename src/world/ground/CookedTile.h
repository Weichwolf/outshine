#ifndef OUTSHINE_WORLD_GROUND_COOKEDTILE_H
#define OUTSHINE_WORLD_GROUND_COOKEDTILE_H

#include <cstdint>
#include <vector>

#include "math/Vec3.h"
#include "ClusterDag.h"

namespace outshine::Ground {

void CookTile(const float *soup,
              int nverts,
              int gridverts,
              const Vec3 &origin,
              std::vector<float> &outVerts,
              std::vector<uint32_t> &outIdx,
              std::vector<DagCluster> &outClusters);

}
#endif
