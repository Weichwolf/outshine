#include "LitterFieldNoise.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <limits>
#include <span>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr uint32_t kLatticeSide = kLitterCellCount + 1;
using Lattice = std::array<uint64_t, size_t{kLatticeSide} * kLatticeSide>;
constexpr uint64_t kSmoothDenominator =
    uint64_t{4} * kLitterSamplesPerCell * kLitterSamplesPerCell * kLitterSamplesPerCell;
constexpr uint64_t kSmoothQuadratic = uint64_t{3} * kLitterSamplesPerCell;
constexpr uint32_t kHashX = 0x9e3779b9u;
constexpr uint32_t kHashY = 0x85ebca6bu;
constexpr uint32_t kHashMixFirst = 0x7feb352du;
constexpr uint32_t kHashMixSecond = 0x846ca68bu;
constexpr uint32_t kHashMask = 0x00ffffffu;
constexpr uint32_t kCellMipLevel = 4;
constexpr uint64_t kQuantizedLatticeDenominator = 65793;

uint64_t LatticeValue(int64_t x, int64_t y) noexcept {
  auto h = static_cast<uint32_t>(uint64_t{static_cast<uint32_t>(x)} * kHashX) ^
           static_cast<uint32_t>(uint64_t{static_cast<uint32_t>(y)} * kHashY);
  h ^= h >> 16u;
  h = static_cast<uint32_t>(uint64_t{h} * kHashMixFirst);
  h ^= h >> 15u;
  h = static_cast<uint32_t>(uint64_t{h} * kHashMixSecond);
  return (h ^ (h >> 16u)) & kHashMask;
}

uint8_t Quantize(uint64_t numerator, uint64_t denominator) noexcept {
  const uint64_t whole = numerator / denominator;
  const uint64_t remainder = numerator % denominator;
  const bool roundUp =
      remainder * 2 > denominator || (remainder * 2 == denominator && (whole & 1u) != 0);
  return static_cast<uint8_t>(whole + static_cast<uint64_t>(roundUp));
}

std::expected<void, std::string>
BuildLattice(Lattice &values, std::array<int32_t, 2> tile, const std::stop_token &stopping) {
  for (uint32_t y = 0; y < kLatticeSide; ++y) {
    if (stopping.stop_requested()) { return std::unexpected("litter mip generation cancelled"); }
    for (uint32_t x = 0; x < kLatticeSide; ++x) {
      values[size_t{y} * kLatticeSide + x] =
          LatticeValue(int64_t{tile[0]} * kLitterCoreCells - kLitterHaloCells + x,
                       int64_t{tile[1]} * kLitterCoreCells - kLitterHaloCells + y);
    }
  }
  return {};
}

std::expected<void, std::string> Fine(const Lattice &values,
                                      uint32_t level,
                                      std::span<uint8_t> pixels,
                                      const std::stop_token &stopping) {
  const uint32_t count = uint32_t{1} << level;
  const uint32_t side = kLitterFineSide >> level;
  std::array<uint64_t, kLitterSamplesPerCell> smooth{};
  for (uint32_t first = 0; first < kLitterSamplesPerCell; first += count) {
    uint64_t sum = 0;
    for (uint32_t at = first; at < first + count; ++at) {
      const uint64_t odd = at * 2 + 1;
      sum += kSmoothQuadratic * odd * odd - odd * odd * odd;
    }
    smooth[first] = sum >> level;
  }
  for (uint32_t y = 0; y < side; ++y) {
    if (stopping.stop_requested()) { return std::unexpected("litter mip generation cancelled"); }
    const uint32_t row = (y * count / kLitterSamplesPerCell) * kLatticeSide;
    const uint64_t sy = smooth[y * count % kLitterSamplesPerCell];
    const uint64_t iy = kSmoothDenominator - sy;
    for (uint32_t x = 0; x < side; ++x) {
      const uint32_t at = row + x * count / kLitterSamplesPerCell;
      const uint64_t sx = smooth[x * count % kLitterSamplesPerCell];
      const uint64_t ix = kSmoothDenominator - sx;
      const uint64_t weighted = values[at] * ix * iy + values[at + 1] * sx * iy +
                                values[at + kLatticeSide] * ix * sy +
                                values[at + kLatticeSide + 1] * sx * sy;
      pixels[size_t{y} * side + x] = Quantize(
          weighted, kSmoothDenominator * kSmoothDenominator * kQuantizedLatticeDenominator);
    }
  }
  return {};
}

std::expected<void, std::string> Coarse(const Lattice &values,
                                        uint32_t level,
                                        std::span<uint8_t> pixels,
                                        const std::stop_token &stopping) {
  Lattice prefix{};
  for (size_t y = 0; y < kLitterCellCount; ++y) {
    if (stopping.stop_requested()) { return std::unexpected("litter mip generation cancelled"); }
    for (size_t x = 0; x < kLitterCellCount; ++x) {
      const size_t at = y * kLatticeSide + x;
      const uint64_t cell =
          values[at] + values[at + 1] + values[at + kLatticeSide] + values[at + kLatticeSide + 1];
      prefix[at + kLatticeSide + 1] =
          cell + prefix[at + 1] + prefix[at + kLatticeSide] - prefix[at];
    }
  }
  const uint32_t count = uint32_t{1} << (level - kCellMipLevel);
  const uint32_t side = kLitterFineSide >> level;
  for (uint32_t y = 0; y < side; ++y) {
    if (stopping.stop_requested()) { return std::unexpected("litter mip generation cancelled"); }
    for (uint32_t x = 0; x < side; ++x) {
      const uint32_t low = y * count * kLatticeSide + x * count;
      const uint32_t high = low + count * kLatticeSide + count;
      const uint64_t sum =
          (prefix[high] - prefix[low + count]) - (prefix[high - count] - prefix[low]);
      pixels[size_t{y} * side + x] =
          Quantize(sum, uint64_t{4} * count * count * kQuantizedLatticeDenominator);
    }
  }
  return {};
}
}

std::optional<uint32_t> LitterMipSide(std::array<int32_t, 2> tile, uint32_t level) noexcept {
  constexpr int32_t low =
      std::numeric_limits<int32_t>::min() / static_cast<int32_t>(kLitterCoreCells) + 1;
  constexpr int32_t high =
      std::numeric_limits<int32_t>::max() / static_cast<int32_t>(kLitterCoreCells) - 1;
  if (level > kLitterMipMax || tile[0] < low || tile[0] > high || tile[1] < low || tile[1] > high) {
    return std::nullopt;
  }
  return kLitterFineSide >> level;
}

std::expected<void, std::string> GenerateLitterMip(std::array<int32_t, 2> tile,
                                                   uint32_t level,
                                                   std::span<uint8_t> pixels,
                                                   const std::stop_token &stopping) {
  const auto side = LitterMipSide(tile, level);
  if (level > kLitterMipMax || !side || pixels.size() != size_t{*side} * *side) {
    return std::unexpected("invalid litter mip address or pixel extent");
  }
  Lattice values;
  auto ready = BuildLattice(values, tile, stopping);
  if (!ready) { return std::unexpected(std::move(ready.error())); }
  return level <= kCellMipLevel ? Fine(values, level, pixels, stopping)
                                : Coarse(values, level, pixels, stopping);
}
}
