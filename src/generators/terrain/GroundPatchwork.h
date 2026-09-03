#ifndef OUTSHINE_GENERATORS_TERRAIN_GROUNDPATCHWORK_H
#define OUTSHINE_GENERATORS_TERRAIN_GROUNDPATCHWORK_H

#include "math/Vec3.h"
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "GroundMesher.h"

namespace outshine {

class Patchworker final : public GroundMesher {
public:
  [[nodiscard]] std::expected<Patchwork, std::string> Lay(TileMeshes &tiles,
                                                          const Around &over) const override;

  void Yield(std::span<const Yields> these,
             Budget within,
             GroundMesh mesh,
             Yielded &told) const override;
};

[[nodiscard]] std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles,
                                                                 const Around &over);

} // namespace outshine

#endif
