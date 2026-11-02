#include "LitterPageNoise.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint32_t kSamplesPerCell = 16;
constexpr uint64_t kSmoothQuadratic = uint64_t{3} * kSamplesPerCell;
constexpr uint32_t kHashX = 0x9e3779b9u;
constexpr uint32_t kHashY = 0x85ebca6bu;
constexpr uint32_t kHashMixFirst = 0x7feb352du;
constexpr uint32_t kHashMixSecond = 0x846ca68bu;
constexpr uint32_t kHashMask = 0x00ffffffu;
constexpr uint64_t kSmoothDenominator = 16384;
constexpr uint64_t kQuantizedLatticeDenominator = 65793;
using Cells = std::array<uint32_t, kLitterPageSideMost>;
using Weights = std::array<uint64_t, kLitterPageSideMost>;
using Smooth = std::array<uint64_t, kSamplesPerCell>;

int64_t FloorDivide(int64_t value, int64_t divisor) noexcept {
  return value / divisor - static_cast<int64_t>(value % divisor < 0);
}

uint32_t LatticeValue(int64_t x, int64_t y) noexcept {
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
  const bool up =
      remainder * 2 > denominator || (remainder * 2 == denominator && (whole & 1u) != 0);
  return static_cast<uint8_t>(whole + static_cast<uint64_t>(up));
}

std::expected<void, std::string> Fine(std::span<const uint32_t> values,
                                      const LitterPageLayout &layout,
                                      uint32_t level,
                                      std::span<uint8_t> pixels,
                                      const std::stop_token &stop) {
  const uint32_t count = uint32_t{1} << level;
  Smooth lookup{};
  for (uint32_t first = 0; first < kSamplesPerCell; first += count) {
    uint64_t sum = 0;
    for (uint32_t at = first; at < first + count; ++at) {
      const uint64_t odd = at * 2 + 1;
      sum += kSmoothQuadratic * odd * odd - odd * odd * odd;
    }
    lookup[first] = sum >> level;
  }
  Cells cells;
  Weights smooth;
  for (uint32_t at = 0; at < layout.Side; ++at) {
    const int64_t fine = (int64_t{at} - 1) * count;
    const int64_t cell = FloorDivide(fine, kSamplesPerCell);
    cells[at] = static_cast<uint32_t>(cell + 1);
    smooth[at] = lookup[static_cast<size_t>(fine - cell * kSamplesPerCell)];
  }
  for (uint32_t y = 0; y < layout.Side; ++y) {
    if (stop.stop_requested()) { return std::unexpected("litter page generation cancelled"); }
    const uint32_t row = cells[y] * layout.LatticeSide;
    const uint64_t sy = smooth[y];
    const uint64_t iy = kSmoothDenominator - sy;
    for (uint32_t x = 0; x < layout.Side; ++x) {
      const uint32_t at = row + cells[x];
      const uint64_t sx = smooth[x];
      const uint64_t ix = kSmoothDenominator - sx;
      const uint64_t sum = uint64_t{values[at]} * ix * iy + uint64_t{values[at + 1]} * sx * iy +
                           uint64_t{values[at + layout.LatticeSide]} * ix * sy +
                           uint64_t{values[at + layout.LatticeSide + 1]} * sx * sy;
      pixels[size_t{y} * layout.Side + x] =
          Quantize(sum, kSmoothDenominator * kSmoothDenominator * kQuantizedLatticeDenominator);
    }
  }
  return {};
}

std::expected<void, std::string> Coarse(std::span<const uint32_t> values,
                                        const LitterPageLayout &layout,
                                        std::span<uint8_t> pixels,
                                        const std::stop_token &stop) {
  const uint32_t stride = layout.LatticeSide;
  std::vector<uint64_t> prefix;
  prefix.reserve(values.size());
  prefix.resize(stride, 0);
  for (uint32_t y = 0; y + 1 < stride; ++y) {
    if (stop.stop_requested()) { return std::unexpected("litter page generation cancelled"); }
    prefix.push_back(0);
    for (uint32_t x = 0; x + 1 < stride; ++x) {
      const size_t at = size_t{y} * stride + x;
      const uint64_t cell =
          uint64_t{values[at]} + values[at + 1] + values[at + stride] + values[at + stride + 1];
      prefix.push_back(cell + prefix[at + 1] + prefix[at + stride] - prefix[at]);
    }
  }
  constexpr uint32_t count = 2;
  for (uint32_t y = 0; y < layout.Side; ++y) {
    if (stop.stop_requested()) { return std::unexpected("litter page generation cancelled"); }
    for (uint32_t x = 0; x < layout.Side; ++x) {
      const size_t low = size_t{y} * count * stride + size_t{x} * count;
      const size_t high = low + size_t{count} * stride + count;
      const uint64_t sum =
          (prefix[high] - prefix[low + count]) - (prefix[high - count] - prefix[low]);
      pixels[size_t{y} * layout.Side + x] =
          Quantize(sum, uint64_t{4} * count * count * kQuantizedLatticeDenominator);
    }
  }
  return {};
}
}

std::optional<LitterPageLayout> PlanLitterPage(LitterPageAddress address) noexcept {
  if (address.Level > kLitterPageMipMax ||
      (address.CoreTexels != 64 && address.CoreTexels != 128)) {
    return std::nullopt;
  }
  LitterPageLayout layout{.Side = address.CoreTexels + 2};
  const uint32_t count = uint32_t{1} << address.Level;
  layout.LatticeSide =
      address.Level <= 4 ? address.CoreTexels * count / kSamplesPerCell + 3 : layout.Side * 2 + 1;
  for (size_t axis = 0; axis < 2; ++axis) {
    const int64_t root = FloorDivide(address.Tile[axis],
                                     int64_t{uint32_t{1} << (kLitterPageMipMax - address.Level)});
    const int64_t first = root * address.CoreTexels * 2 - 2;
    const int64_t last = root * address.CoreTexels * 2 + int64_t{address.CoreTexels} * 2 + 2;
    if (first < std::numeric_limits<int32_t>::min() || last > std::numeric_limits<int32_t>::max()) {
      return std::nullopt;
    }
    layout.FirstCell[axis] =
        address.Level <= 4
            ? int64_t{address.Tile[axis]} * address.CoreTexels * count / kSamplesPerCell - 1
            : (int64_t{address.Tile[axis]} * address.CoreTexels - 1) * 2;
  }
  const size_t sites = size_t{layout.LatticeSide} * layout.LatticeSide;
  layout.WorkingBytesMost = sites * sizeof(uint32_t) +
                            (address.Level <= 4 ? sizeof(Cells) + sizeof(Weights) + sizeof(Smooth)
                                                : sites * sizeof(uint64_t));
  return layout;
}

std::optional<LitterPageAddress> LitterPageParent(LitterPageAddress address) noexcept {
  if (!PlanLitterPage(address) || address.Level == kLitterPageMipMax) { return std::nullopt; }
  for (auto &tile : address.Tile) { tile = static_cast<int32_t>(FloorDivide(tile, 2)); }
  ++address.Level;
  return address;
}

std::expected<void, std::string> GenerateLitterPage(LitterPageAddress address,
                                                    std::span<uint8_t> pixels,
                                                    const std::stop_token &stopping) {
  const auto layout = PlanLitterPage(address);
  if (!layout || pixels.size() != size_t{layout->Side} * layout->Side) {
    return std::unexpected("invalid litter page address or pixel extent");
  }
  if (stopping.stop_requested()) { return std::unexpected("litter page generation cancelled"); }
  const size_t sites = size_t{layout->LatticeSide} * layout->LatticeSide;
  std::vector<uint32_t> lattice;
  lattice.reserve(sites);
  for (uint32_t y = 0; y < layout->LatticeSide; ++y) {
    if (stopping.stop_requested()) { return std::unexpected("litter page generation cancelled"); }
    for (uint32_t x = 0; x < layout->LatticeSide; ++x) {
      lattice.push_back(LatticeValue(layout->FirstCell[0] + x, layout->FirstCell[1] + y));
    }
  }
  const std::span values(lattice);
  return address.Level <= 4 ? Fine(values, *layout, address.Level, pixels, stopping)
                            : Coarse(values, *layout, pixels, stopping);
}
}
