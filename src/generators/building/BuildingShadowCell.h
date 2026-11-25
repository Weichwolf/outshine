#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSHADOWCELL_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSHADOWCELL_H
#include "Box.h"
#include "StoredVertex.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine {
class AssetCache;
}

namespace outshine::Generators {
struct BuildingShadowCell {
  struct Part {
    std::vector<StoredVertex> Vertices;
    std::vector<uint32_t> Indices;
  };

  struct ReadCost {
    size_t PacketBytes = 0, OriginalBytes = 0;
  };

  std::string Key;
  Box Bounds;
  double ErrorM = 0;
  uint64_t Digest = 0;
  std::array<Part, 2> Parts;

  [[nodiscard]] static std::optional<BuildingShadowCell>
  Resolve(AssetCache &cache,
          const std::string &parent,
          uint32_t cell,
          std::span<const std::string> originalKeys,
          ReadCost &cost);
};
}
#endif
