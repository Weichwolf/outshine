#ifndef OUTSHINE_GENERATORS_TERRAIN_LITTERPAGENOISE_H
#define OUTSHINE_GENERATORS_TERRAIN_LITTERPAGENOISE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>

namespace outshine::Generators {
inline constexpr uint32_t kLitterPageMipMax = 5;
inline constexpr uint32_t kLitterPageCoreMost = 128;
inline constexpr uint32_t kLitterPageSideMost = kLitterPageCoreMost + 2;

struct LitterPageAddress {
  std::array<int32_t, 2> Tile{};
  uint32_t Level = 0;
  uint32_t CoreTexels = 64;
};

struct LitterPageLayout {
  uint32_t Side = 0;
  uint32_t LatticeSide = 0;
  std::array<int64_t, 2> FirstCell{};
  size_t WorkingBytesMost = 0;
};

[[nodiscard]] std::optional<LitterPageLayout> PlanLitterPage(LitterPageAddress address) noexcept;
[[nodiscard]] std::optional<LitterPageAddress> LitterPageParent(LitterPageAddress address) noexcept;

[[nodiscard]] std::expected<void, std::string> GenerateLitterPage(
    LitterPageAddress address, std::span<uint8_t> pixels, const std::stop_token &stopping = {});
}
#endif
