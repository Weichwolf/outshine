#include "Calibration.h"
#include "Parameters.inc"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace outshine::Diagnostics::Calibration {
FmaValue FmaInput(uint32_t index) noexcept {
  FmaValue value{};
  for (uint32_t lane = 0; lane < value.size(); ++lane) {
    value[lane] = std::bit_cast<float>(
        CALIBRATION_FMA_EXPONENT |
        ((index * CALIBRATION_MIX_MULTIPLIER + lane * CALIBRATION_MIX_INCREMENT) &
         CALIBRATION_FMA_MANTISSA));
  }
  return value;
}

FmaValue FmaReference(FmaValue value) noexcept {
  for (uint32_t step = 0; step < CALIBRATION_FMA_STEPS; ++step) {
    value[0] = std::fma(value[0], CALIBRATION_SCALE0, CALIBRATION_OFFSET0);
    value[1] = std::fma(value[1], CALIBRATION_SCALE1, CALIBRATION_OFFSET1);
    value[2] = std::fma(value[2], CALIBRATION_SCALE2, CALIBRATION_OFFSET2);
    value[3] = std::fma(value[3], CALIBRATION_SCALE3, CALIBRATION_OFFSET3);
  }
  return value;
}

void FmaKernel(std::span<const FmaValue> input, std::span<FmaValue> output) noexcept {
  for (size_t at = 0; at < input.size(); ++at) { output[at] = FmaReference(input[at]); }
}

void TriadKernel(std::span<const uint32_t> a,
                 std::span<const uint32_t> b,
                 std::span<uint32_t> into) noexcept {
  for (size_t at = 0; at < into.size(); ++at) { into[at] = a[at] + 3u * b[at]; }
}

uint32_t ChaseKernel(std::span<const uint32_t> next, uint32_t steps) noexcept {
  uint32_t at = 0;
  for (uint32_t step = 0; step < steps; ++step) { at = next[at]; }
  return at;
}

uint32_t RasterValue(uint32_t x, uint32_t y) noexcept {
  y = y < CALIBRATION_HEIGHT / 2u ? y : CALIBRATION_HEIGHT - 1u - y;
  uint32_t value = x + CALIBRATION_WIDTH * y;
  for (uint32_t step = 0; step < CALIBRATION_RASTER_STEPS; ++step) {
    value = value * CALIBRATION_MIX_MULTIPLIER + CALIBRATION_MIX_INCREMENT;
    value ^= value >> 13u;
  }
  return value;
}

uint64_t Checksum(std::span<const std::byte> bytes) noexcept {
  constexpr uint64_t offsetBasis = 14695981039346656037ull;
  constexpr uint64_t prime = 1099511628211ull;
  uint64_t value = offsetBasis;
  for (const auto byte : bytes) { value = (value ^ std::to_integer<uint8_t>(byte)) * prime; }
  return value;
}
}
