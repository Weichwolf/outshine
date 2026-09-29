#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREARTIFACT_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREARTIFACT_H

#include "StructureBake.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <string>
#include <vector>

namespace outshine::Generators {

inline constexpr size_t kStructureArtifactBytesMost = size_t{64} * 1024 * 1024;

[[nodiscard]] std::optional<std::string>
StructureArtifactKey(const RawTile &raw,
                     const Ground::HeightField &heights,
                     const std::optional<Data::TileSourceIdentity> &source,
                     std::string_view producerVersion);

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeStructureArtifact(const BakedTile &tile, std::string_view inputKey);

[[nodiscard]] std::optional<BakedTile> DecodeStructureArtifact(std::span<const uint8_t> bytes,
                                                               std::string_view inputKey,
                                                               uint64_t currentSourceKey);

}
#endif
