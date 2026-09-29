#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREARTIFACT_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREARTIFACT_H

#include "StructureBake.h"
#include <cstdint>
#include <functional>
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

enum class StructureArtifactError {
  InvalidKey,
  InvalidProduct,
  InvalidScalar,
  CapacityExceeded,
  WriteFailed
};

struct StructureArtifactReadLimits {
  size_t EncodedBytes = 0;
  size_t ResidentBytesMost = 0;
};

using StructureArtifactSink = std::function<bool(std::span<const uint8_t>)>;
using StructureArtifactSource = std::function<bool(std::span<uint8_t>)>;

[[nodiscard]] std::expected<void, StructureArtifactError>
WriteStructureProduct(const BakedTile &tile, const StructureArtifactSink &sink, size_t blockBytes);

[[nodiscard]] std::optional<BakedTile> ReadStructureProduct(const StructureArtifactSource &source,
                                                            StructureArtifactReadLimits limits,
                                                            uint64_t currentSourceKey);

[[nodiscard]] std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodeStructureArtifact(const BakedTile &tile, std::string_view inputKey);

[[nodiscard]] std::optional<BakedTile> DecodeStructureArtifact(std::span<const uint8_t> bytes,
                                                               std::string_view inputKey,
                                                               uint64_t currentSourceKey);

}
#endif
