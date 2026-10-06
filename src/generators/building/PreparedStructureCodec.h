#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTURECODEC_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTURECODEC_H

#include "PreparedStructureTile.h"
#include "StructureArtifact.h"

namespace outshine::Generators {

[[nodiscard]] std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedStructureTile(const PreparedStructureTile &tile);

[[nodiscard]] std::optional<PreparedStructureTile>
DecodePreparedStructureTile(std::span<const uint8_t> bytes, size_t residentBytesMost);

}
#endif
