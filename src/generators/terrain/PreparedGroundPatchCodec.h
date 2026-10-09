#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDGROUNDPATCHCODEC_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDGROUNDPATCHCODEC_H

#include "GroundPatch.h"
#include "world/data/Address.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators {
inline constexpr int kPreparedGroundPatchSideMost = 512;
inline constexpr size_t kPreparedGroundPatchBytesMost =
    static_cast<size_t>(kPreparedGroundPatchSideMost) * kPreparedGroundPatchSideMost *
        sizeof(double) +
    64;

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodePreparedGroundPatch(Data::TileId at, const GroundPatch &patch);

[[nodiscard]] std::shared_ptr<const GroundPatch>
DecodePreparedGroundPatch(Data::TileId at, int side, std::span<const uint8_t> bytes);
}
#endif
