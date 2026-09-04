#ifndef OUTSHINE_WORLD_GROUND_TILEMESHES_H
#define OUTSHINE_WORLD_GROUND_TILEMESHES_H

#include <cstdint>
#include <vector>

#include "Address.h"
#include <array>
#include <cstddef>
#include <type_traits>
#include "math/Vec3.h"
#include "ClusterDag.h"

namespace outshine {

struct TileVertex {
  std::array<float, 3> PlaceM;
  std::array<float, 2> Uv;
  std::array<float, 3> Facing;
};

static_assert(std::is_trivially_copyable_v<TileVertex>);
static_assert(std::is_standard_layout_v<TileVertex>);
static_assert(sizeof(TileVertex) == 8 * sizeof(float),
              "no padding: a soup of these is a float run");
static_assert(offsetof(TileVertex, PlaceM) == 0);
static_assert(offsetof(TileVertex, Uv) == 3 * sizeof(float));
static_assert(offsetof(TileVertex, Facing) == 5 * sizeof(float));

inline constexpr size_t kTileVertexFloats = sizeof(TileVertex) / sizeof(float);

inline constexpr size_t kTileVertexUvAt = offsetof(TileVertex, Uv) / sizeof(float);
inline constexpr size_t kTileVertexFacingAt = offsetof(TileVertex, Facing) / sizeof(float);

static_assert(kTileVertexFloats == 8);
static_assert(kTileVertexUvAt == 3);
static_assert(kTileVertexFacingAt == 5);

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
