#ifndef OUTSHINE_WORLD_GROUND_TILEMESHES_H
#define OUTSHINE_WORLD_GROUND_TILEMESHES_H

#include <cstdint>
#include <vector>

#include "ClusterDag.h"

namespace outshine {

inline constexpr size_t kTileVertexFloats = 8;

struct TileBuild {
  std::vector<float> Verts;
  std::vector<uint32_t> Idx;
  std::vector<DagCluster> Clusters;
  double OriginEcef[3] = {0.0, 0.0, 0.0};
  float ErrM = 0.0f;
};

class TileMeshes {
public:
  enum class Reply { Ready, Pending, Absent, Refused, Undeclared };

  virtual ~TileMeshes() = default;

  [[nodiscard]] virtual Reply Mesh(int z, uint32_t x, uint32_t y, int grid, TileBuild *out) = 0;

  [[nodiscard]] virtual Reply
  MeshAwaited(int z, uint32_t x, uint32_t y, int grid, TileBuild *out) = 0;

  [[nodiscard]] virtual Reply Wants(int z, uint32_t x, uint32_t y, int grid) = 0;
};

} // namespace outshine
#endif
