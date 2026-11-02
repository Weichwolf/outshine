#ifndef OUTSHINE_GENERATORS_TERRAIN_LITTERFIELDNOISE_H
#define OUTSHINE_GENERATORS_TERRAIN_LITTERFIELDNOISE_H

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>

namespace outshine::Generators {
inline constexpr uint32_t kLitterCellCount = 32;
inline constexpr uint32_t kLitterSamplesPerCell = 16;
inline constexpr uint32_t kLitterCoreCells = 16;
inline constexpr uint32_t kLitterHaloCells = (kLitterCellCount - kLitterCoreCells) / 2;
inline constexpr uint32_t kLitterFineSide = kLitterCellCount * kLitterSamplesPerCell;
inline constexpr uint32_t kLitterMipMax = 9;

[[nodiscard]] std::optional<uint32_t> LitterMipSide(std::array<int32_t, 2> tile,
                                                    uint32_t level) noexcept;

[[nodiscard]] std::expected<void, std::string>
GenerateLitterMip(std::array<int32_t, 2> tile,
                  uint32_t level,
                  std::span<uint8_t> pixels,
                  const std::stop_token &stopping = {});
}
#endif
