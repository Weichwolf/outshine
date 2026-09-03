#ifndef OUTSHINE_GENERATORS_FLORA_TREERANDOM_H
#define OUTSHINE_GENERATORS_FLORA_TREERANDOM_H

#include <cstdint>

namespace outshine::Generators {

constexpr float kMantissaSteps = 16777216.0f;

constexpr unsigned kXorShift = 17u;

constexpr uint32_t kGoldenWord = 0x9e3779b9u;

class TreeRandom {
public:
  explicit TreeRandom(uint32_t seed) : State_((seed != 0u) ? seed : kGoldenWord) {}

  uint32_t Next() {
    uint32_t x = State_;
    x ^= x << 13u;
    x ^= x >> kXorShift;
    x ^= x << 5u;
    State_ = x;
    return x;
  }

  float Unit() { return static_cast<float>(Next() >> 8u) * (1.0f / kMantissaSteps); }

  float Signed() { return Unit() * 2.0f - 1.0f; }

private:
  uint32_t State_;
};

} // namespace outshine::Generators
#endif
