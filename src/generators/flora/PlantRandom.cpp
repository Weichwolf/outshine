#include "PlantRandom.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace outshine::Generators {
namespace {
constexpr uint32_t kHashInitial = 0x43b0d7e5, kHashMultiplier = 0x931e8875;
constexpr uint32_t kStateInitial = 0x8b51f9dd, kStateMultiplier = 0x58f38ded;
constexpr uint32_t kMixLeft = 0xca01f9dd, kMixRight = 0x4973f715;
constexpr uint64_t kMultiplierHigh = 2549297995355413924ULL;
constexpr uint64_t kMultiplierLow = 4865540595714422341ULL;
constexpr unsigned kHalfBits = 32, kMixShift = 16, kRotationShift = 58, kCarryShift = 63;

uint32_t Hash(uint32_t value, uint32_t &constant) noexcept {
  value ^= constant;
  constant *= kHashMultiplier;
  value *= constant;
  return value ^ (value >> kMixShift);
}

std::array<uint64_t, 4> InitialState(std::span<const uint64_t> entropy) noexcept {
  std::array<uint32_t, 4> words{};
  size_t count = 0;
  for (const uint64_t value : entropy) {
    words[count++] = static_cast<uint32_t>(value);
    if ((value >> kHalfBits) != 0) { words[count++] = static_cast<uint32_t>(value >> kHalfBits); }
  }
  uint32_t constant = kHashInitial;
  std::array<uint32_t, 4> pool{};
  for (size_t at = 0; at < pool.size(); ++at) { pool[at] = Hash(words[at], constant); }
  for (size_t source = 0; source < pool.size(); ++source) {
    for (size_t destination = 0; destination < pool.size(); ++destination) {
      if (source == destination) { continue; }
      const uint32_t mixed =
          kMixLeft * pool[destination] - kMixRight * Hash(pool[source], constant);
      pool[destination] = mixed ^ (mixed >> kMixShift);
    }
  }
  std::array<uint64_t, 4> result{};
  constant = kStateInitial;
  for (size_t at = 0; at < result.size() * 2; ++at) {
    uint32_t value = pool[at % pool.size()] ^ constant;
    constant *= kStateMultiplier;
    value *= constant;
    value ^= value >> kMixShift;
    result[at / 2] |= static_cast<uint64_t>(value) << ((at % 2) * kHalfBits);
  }
  return result;
}

uint64_t LowProductCarry(uint64_t value) noexcept {
  const uint64_t aLow = static_cast<uint32_t>(value);
  const uint64_t aHigh = value >> kHalfBits;
  constexpr uint64_t bLow = static_cast<uint32_t>(kMultiplierLow);
  constexpr uint64_t bHigh = kMultiplierLow >> kHalfBits;
  const uint64_t first = aLow * bLow;
  const uint64_t middle = aHigh * bLow + (first >> kHalfBits);
  const uint64_t last = aLow * bHigh + static_cast<uint32_t>(middle);
  return aHigh * bHigh + (middle >> kHalfBits) + (last >> kHalfBits);
}
}

PlantRandom::PlantRandom(uint64_t seed) noexcept {
  Seed(std::array{seed});
}

PlantRandom::PlantRandom(uint64_t seed, uint64_t axis) noexcept {
  Seed(std::array{seed, axis});
}

void PlantRandom::Seed(std::span<const uint64_t> entropy) noexcept {
  const auto words = InitialState(entropy);
  Increment_ = {(words[2] << 1) | (words[3] >> kCarryShift), (words[3] << 1) | 1};
  State_ = Increment_;
  const uint64_t before = State_[1];
  State_[1] += words[1];
  State_[0] += words[0] + (State_[1] < before ? 1 : 0);
  Advance();
}

void PlantRandom::Advance() noexcept {
  const uint64_t low = State_[1] * kMultiplierLow;
  const uint64_t high =
      State_[0] * kMultiplierLow + State_[1] * kMultiplierHigh + LowProductCarry(State_[1]);
  State_[1] = low + Increment_[1];
  State_[0] = high + Increment_[0] + (State_[1] < low ? 1 : 0);
}

uint64_t PlantRandom::Raw() noexcept {
  Advance();
  return std::rotr(State_[0] ^ State_[1], static_cast<int>(State_[0] >> kRotationShift));
}

double PlantRandom::Uniform(double minimum, double maximum) noexcept {
  constexpr unsigned kDiscardBits = 11;
  constexpr double kUnit = 0x1.0p-53;
  return minimum + (maximum - minimum) * (static_cast<double>(Raw() >> kDiscardBits) * kUnit);
}
}
