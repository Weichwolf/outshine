#ifndef OUTSHINE_GENERATORS_TERRAIN_GROUNDPATCHWORK_H
#define OUTSHINE_GENERATORS_TERRAIN_GROUNDPATCHWORK_H

#include "TileGeodesy.h"
#include "Vec3.h"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "GroundMesher.h"

namespace outshine {

class Patchworker final : public GroundMesher {
public:
  [[nodiscard]] std::expected<Patchwork, std::string> Lay(TileMeshes &tiles,
                                                          const Around &over) const override;
};

[[nodiscard]] std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles,
                                                                 const Around &over);

[[nodiscard]] std::expected<std::vector<Data::TileId>, std::string>
PlanPatchworkTiles(const Around &over);

[[nodiscard]] std::vector<Data::TileId>
RemoveCoveredPatchworkTiles(std::span<const Data::TileId> requested);

[[nodiscard]] bool
SweptPatchworkCovers(const TerrainSweep &sweep, const Around &request, const Vec3 &eyeM);

[[nodiscard]] std::optional<Ground::GeoBounds> PatchworkFocusWindow(const Around &coverage);

[[nodiscard]] bool PatchworkIncludesContactTile(const Around &coverage,
                                                const Around &request) noexcept;

[[nodiscard]] bool
PatchworkCoversFocus(const Around &coverage, const Around &request, double radiusM) noexcept;

}

#endif
