#ifndef OUTSHINE_WORLD_GROUND_TILEMESHES_H
#define OUTSHINE_WORLD_GROUND_TILEMESHES_H

#include <cstdint>
#include <vector>

#include "Address.h"
#include "math/Vec3.h"
#include "ClusterDag.h"

namespace outshine {

inline constexpr size_t kTileVertexFloats = 8;

struct TileBuild {
  std::vector<float> Verts;
  std::vector<uint32_t> Idx;
  std::vector<DagCluster> Clusters;
  Vec3 OriginEcef;
  float ErrM = 0.0f;
};

class TileMeshes {
public:
  enum class Reply { Ready, Pending, Absent, Refused, Undeclared };

  virtual ~TileMeshes() = default;

  [[nodiscard]] virtual Reply Mesh(Data::TileId of, int grid, TileBuild *out) = 0;

  [[nodiscard]] virtual Reply MeshAwaited(Data::TileId of, int grid, TileBuild *out) = 0;

  [[nodiscard]] virtual Reply Wants(Data::TileId of, int grid) = 0;
};

} // namespace outshine
#endif
