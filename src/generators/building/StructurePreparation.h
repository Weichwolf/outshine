#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREPREPARATION_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREPREPARATION_H

#include "PreparedStructureTile.h"

namespace outshine::Generators {

[[nodiscard]] std::vector<WayLine> StructureWays(const RawTile &raw);

[[nodiscard]] std::expected<std::optional<PreparedStructure>, StructureBakeError>
EnrichStructure(const RawTile &raw,
                const Ground::HeightField &heights,
                const RawTile::Structure &one,
                std::span<const double> points,
                std::span<const WayLine> ways,
                std::vector<double> &corners,
                BakedTile &diagnostics,
                const std::atomic_bool *stopping);

}
#endif
