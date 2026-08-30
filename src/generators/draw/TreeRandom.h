#ifndef OUTSHINE_GENERATORS_DRAW_TREERANDOM_H
#define OUTSHINE_GENERATORS_DRAW_TREERANDOM_H

#include <cstdint>

namespace outshine::Generators {

class TreeRandom {
public:
  explicit TreeRandom(uint32_t seed) : State_(seed ? seed : 0x9e3779b9u) {}

  uint32_t Next() {
    uint32_t x = State_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    State_ = x;
    return x;
  }

  float Unit() { return (float)(Next() >> 8) * (1.0f / 16777216.0f); }

  float Signed() { return Unit() * 2.0f - 1.0f; }

private:
  uint32_t State_;
};

}
#endif
