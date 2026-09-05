#ifndef OUTSHINE_WORLD_GROUND_TILEMESHES_H
#define OUTSHINE_WORLD_GROUND_TILEMESHES_H

#include <cstdint>
#include <vector>

#include "Address.h"
#include <array>
#include <cstddef>
#include <type_traits>

namespace outshine {

struct TileBuild {
  std::vector<float> Nodes;
  int Side = 0;
  uint32_t Postings = 0;
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
