#ifndef OUTSHINE_EXPERIMENT_RECEIVER_BLOCK_OUTWARD_H
#define OUTSHINE_EXPERIMENT_RECEIVER_BLOCK_OUTWARD_H
#include <bit>
#include <cstdint>
#include <limits>

namespace ReceiverBlocks {
inline constexpr int Binary64Precision = 53;
inline constexpr uint64_t OneEncoding = 0x3ff0000000000000;
inline constexpr uint64_t NegativeZeroEncoding = 0x8000000000000000;

static_assert(sizeof(double) == sizeof(uint64_t));
static_assert(std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == Binary64Precision);
static_assert(std::bit_cast<uint64_t>(1.) == OneEncoding);
static_assert(std::bit_cast<uint64_t>(-0.) == NegativeZeroEncoding);

[[nodiscard]] inline double NextFinite(double value, bool positive) noexcept {
  constexpr uint64_t Sign = uint64_t{1} << 63;
  auto bits = std::bit_cast<uint64_t>(value);
  if ((bits & ~Sign) == 0) {
    bits = positive ? uint64_t{1} : Sign | 1;
  } else if (((bits & Sign) == 0) == positive) {
    ++bits;
  } else {
    --bits;
  }
  return std::bit_cast<double>(bits);
}
}
#endif
