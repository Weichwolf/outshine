#ifndef OUTSHINE_GENERATORS_DRAW_TREERANDOM_H
#define OUTSHINE_GENERATORS_DRAW_TREERANDOM_H

#include <cstdint>

namespace outshine::Generators {

class TreeRandom {
public:
  explicit TreeRandom(uint32_t seed) : State_((seed != 0u) ? seed : 0x9e3779b9u) {}

  uint32_t Next() {
    uint32_t x = State_;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    State_ = x;
    return x;
  }

  float Unit() { return static_cast<float>(Next() >> 8u) * (1.0f / 16777216.0f); }

  float Signed() { return Unit() * 2.0f - 1.0f; }

private:
  uint32_t State_;
};

} // namespace outshine::Generators
#endif
