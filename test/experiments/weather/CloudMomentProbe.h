#ifndef OUTSHINE_TEST_WEATHER_CLOUDMOMENTPROBE_H
#define OUTSHINE_TEST_WEATHER_CLOUDMOMENTPROBE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace outshine::Test::CloudMomentProbe {
using Vector = std::array<float, 4>;
inline constexpr size_t kCells = 32;

struct alignas(16) Query {
  Vector Shell;
  Vector Field;
  Vector Drift;
  Vector OriginQueryM;
  Vector DirectionExtentM;
  Vector Control;
  std::array<Vector, kCells> Cells;
};

struct alignas(16) Answer {
  Vector Moments;
  std::array<Vector, kCells> Cells;
};

static_assert(std::is_trivially_copyable_v<Query> && alignof(Query) == 16);
static_assert(sizeof(Query) == (6 + kCells) * sizeof(Vector));
static_assert(offsetof(Query, Shell) == 0 && offsetof(Query, Field) == sizeof(Vector) &&
              offsetof(Query, Drift) == 2 * sizeof(Vector) &&
              offsetof(Query, OriginQueryM) == 3 * sizeof(Vector) &&
              offsetof(Query, DirectionExtentM) == 4 * sizeof(Vector) &&
              offsetof(Query, Control) == 5 * sizeof(Vector) &&
              offsetof(Query, Cells) == 6 * sizeof(Vector));
static_assert(std::is_trivially_copyable_v<Answer> && alignof(Answer) == 16);
static_assert(sizeof(Answer) == (1 + kCells) * sizeof(Vector) && offsetof(Answer, Moments) == 0 &&
              offsetof(Answer, Cells) == sizeof(Vector));
}
#endif
