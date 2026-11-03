#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDLITTERPAGE_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDLITTERPAGE_H

#include "AssetPackage.h"
#include "LitterPageNoise.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>

namespace outshine::Generators {
struct LitterPageView {
  uint32_t Side = 0;
  std::span<const uint8_t> Pixels;
};

inline constexpr size_t kLitterPageHeaderBytes = 16;
inline constexpr size_t kLitterPageBytesMost =
    kLitterPageHeaderBytes + size_t{kLitterPageSideMost} * kLitterPageSideMost;

// Addresses/bounds use procedure lattice units; metric placement is a consumer binding.
[[nodiscard]] std::expected<std::string, std::string> LitterPageKey(LitterPageAddress address);

[[nodiscard]] std::expected<AssetPackage, std::string> PrepareLitterPage(
    LitterPageAddress address, size_t packageBytesMost, const std::stop_token &stopping = {});

// The returned view borrows its package; retain that owner through its final consumer.
[[nodiscard]] std::expected<LitterPageView, std::string>
DecodeLitterPage(LitterPageAddress address, std::span<const uint8_t> package);
}
#endif
