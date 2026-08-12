/* The growth's ONE source of chance: xorshift32, seeded from the species' declared seed. Deterministic
 * and portable — the same declaration yields the same tree in the browser and natively, which is what
 * makes a vertex count an acceptance criterion at all. */
#ifndef TREERANDOM_H
#define TREERANDOM_H

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

} // namespace outshine::Generators
#endif
