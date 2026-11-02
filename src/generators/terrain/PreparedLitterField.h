#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDLITTERFIELD_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDLITTERFIELD_H

#include "AssetGeneration.h"
#include "LitterFieldNoise.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>

namespace outshine::Generators {
struct LitterFieldAddress {
  std::array<int32_t, 2> Tile{};
  uint32_t Level = 0;
};

struct LitterFieldMipView {
  uint32_t Side = 0;
  std::span<const uint8_t> Pixels;
};

inline constexpr size_t kLitterFieldHeaderBytes = 16;
inline constexpr size_t kLitterFieldBytesMost =
    kLitterFieldHeaderBytes + size_t{kLitterFineSide} * kLitterFineSide;

// Addresses/bounds use procedure lattice units; metric placement is a consumer binding.
[[nodiscard]] std::expected<std::string, std::string> LitterFieldKey(LitterFieldAddress address);

[[nodiscard]] std::expected<GeneratedAssetPackage, std::string> PrepareLitterField(
    LitterFieldAddress address, size_t packageBytesMost, const std::stop_token &stopping = {});

// The returned view borrows its package; retain that owner through its final consumer.
[[nodiscard]] std::expected<LitterFieldMipView, std::string>
DecodeLitterField(LitterFieldAddress address, std::span<const uint8_t> package);
}
#endif
