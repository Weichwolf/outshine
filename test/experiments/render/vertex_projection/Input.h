#ifndef OUTSHINE_EXPERIMENTS_PROJECTION_INPUT_H
#define OUTSHINE_EXPERIMENTS_PROJECTION_INPUT_H
#include <array>
#include <cstddef>
#include <cstdint>

namespace Projection {
using Matrix = std::array<float, 16>;
using Vector = std::array<float, 4>;

struct alignas(16) Input {
  Matrix Model{};
  Matrix View{};
  Vector Shift{};
  Vector Point{};
  std::array<uint32_t, 4> Options{};
};

static_assert(sizeof(Input) == sizeof(Matrix) * 2 + sizeof(Vector) * 3 && alignof(Input) == 16);
static_assert(offsetof(Input, View) == 64 && offsetof(Input, Shift) == 128);
static_assert(offsetof(Input, Point) == sizeof(Matrix) * 2 + sizeof(Vector) &&
              offsetof(Input, Options) == sizeof(Matrix) * 2 + sizeof(Vector) * 2);

}
#endif
