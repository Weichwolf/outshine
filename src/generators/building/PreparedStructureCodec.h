#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTURECODEC_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTURECODEC_H

#include "PreparedStructureTile.h"
#include "PreparedBuildingBasis.h"
#include "StructureArtifact.h"

namespace outshine::Generators {

[[nodiscard]] std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedBuildingBasis(const PreparedBuildingBasis &basis);
[[nodiscard]] std::optional<PreparedBuildingBasis>
DecodePreparedBuildingBasis(std::span<const uint8_t> bytes, size_t residentBytesMost);

[[nodiscard]] std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedStructureTile(const PreparedStructureTile &tile);

[[nodiscard]] std::optional<PreparedStructureTile>
DecodePreparedStructureTile(std::span<const uint8_t> bytes, size_t residentBytesMost);

[[nodiscard]] std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedStructureIndex(const PreparedStructureTile &tile);
[[nodiscard]] std::optional<PreparedStructureTile>
DecodePreparedStructureIndex(std::span<const uint8_t> bytes, size_t residentBytesMost);
[[nodiscard]] std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodeBuildingSurfaceBlock(std::span<const BuildingSurface *const> surfaces);
[[nodiscard]] std::optional<std::vector<BuildingSurface>>
DecodeBuildingSurfaceBlock(std::span<const uint8_t> bytes, size_t residentBytesMost);
[[nodiscard]] bool CompatiblePreparedSurface(const BuildingSurface &header,
                                             const BuildingSurface &model) noexcept;

}
#endif
