#include "Rotation.h"
#include "math/Mat4.h"
#include "math/Quat.h"
#include <array>
#include <cmath>
#include <cstddef>

namespace outshine {

Quat UnitRotationOf(const Mat4 &matrix) noexcept {
  const double trace = matrix[0] + matrix[5] + matrix[10];
  if (trace > 0) {
    const double scale = 2 * std::sqrt(trace + 1);
    return {.X = (matrix[6] - matrix[9]) / scale,
            .Y = (matrix[8] - matrix[2]) / scale,
            .Z = (matrix[1] - matrix[4]) / scale,
            .W = scale / 4};
  }
  std::array<double, 4> values{};
  size_t axis = 0;
  for (size_t at = 1; at < 3; ++at) {
    if (matrix[at * 5] > matrix[axis * 5]) { axis = at; }
  }
  const size_t next = (axis + 1) % 3;
  const size_t last = (next + 1) % 3;
  const double scale = 2 * std::sqrt(1 + matrix[axis * 5] - matrix[next * 5] - matrix[last * 5]);
  values[axis] = scale / 4;
  values[next] = (matrix[axis * 4 + next] + matrix[next * 4 + axis]) / scale;
  values[last] = (matrix[axis * 4 + last] + matrix[last * 4 + axis]) / scale;
  values[3] = (matrix[next * 4 + last] - matrix[last * 4 + next]) / scale;
  return {.X = values[0], .Y = values[1], .Z = values[2], .W = values[3]};
}

}
