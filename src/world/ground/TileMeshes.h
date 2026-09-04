#ifndef OUTSHINE_WORLD_GROUND_TILEMESHES_H
#define OUTSHINE_WORLD_GROUND_TILEMESHES_H

#include <cstdint>
#include <vector>

#include "Address.h"
#include <array>
#include <cstddef>
#include <type_traits>
#include "math/Vec3.h"
#include "ChunkVtx.h"
#include "ClusterDag.h"

namespace outshine {

using TileVertex = ChunkVtx;

inline constexpr size_t kTileVertexFloats = kChunkVtxFloats;
inline constexpr size_t kTileVertexUvAt = kChunkVtxUvAt;
inline constexpr size_t kTileVertexFacingAt = kChunkVtxNormAt;

struct TileBuild {
  std::vector<TileVertex> Verts;
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
