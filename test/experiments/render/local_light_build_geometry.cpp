#include "local_light_build.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace LightBuild {
Vector Camera::View(const Light &light) const {
  Vector result{};
  for (size_t row = 0; row < 3; ++row) {
    for (size_t axis = 0; axis < 3; ++axis) {
      result[row] +=
          (static_cast<double>(light[4 + axis]) - Values[9 + axis]) * Values[row * 3 + axis];
    }
  }
  return result;
}

namespace {
std::array<double, 2> Ratios(double centre, double radius, double near, double far) {
  const std::array values{(centre - radius) / near,
                          (centre - radius) / far,
                          (centre + radius) / near,
                          (centre + radius) / far};
  const auto [low, high] = std::ranges::minmax_element(values);
  return {*low, *high};
}

std::array<double, 2> Products(double low, double high, double near, double far) {
  const std::array values{low * near, low * far, high * near, high * far};
  const auto [first, last] = std::ranges::minmax_element(values);
  return {*first, *last};
}
}

Bounds Project(const Camera &camera,
               std::span<const double> edges,
               const Vector &position,
               double radius) {
  const double near = position[2] - radius;
  const double far = position[2] + radius;
  if (far < edges.front()) { return {}; }
  std::array<double, 2> low{};
  std::array<double, 2> high{};
  for (size_t axis = 0; axis < 2; ++axis) {
    if (camera.Perspective && near <= edges.front()) {
      low[axis] = -1;
      high[axis] = 1;
    } else if (camera.Perspective) {
      const auto values = Ratios(position[axis], radius, near, far);
      low[axis] = values[0] / camera.Values[12 + axis];
      high[axis] = values[1] / camera.Values[12 + axis];
    } else {
      low[axis] = (position[axis] - radius) / camera.Values[12 + axis];
      high[axis] = (position[axis] + radius) / camera.Values[12 + axis];
    }
    if (high[axis] < -1 || low[axis] > 1) { return {}; }
  }
  Bounds result;
  for (size_t axis = 0; axis < 2; ++axis) {
    const double first = axis == 0 ? low[0] : -high[1];
    const double last = axis == 0 ? high[0] : -low[1];
    const double pixels = camera.Grid[axis];
    // Grid stores pixel dimensions here; division precedes the final tile clamp.
    result.First[axis] =
        static_cast<int>(std::clamp(std::floor((first * 0.5 + 0.5) * pixels / camera.Grid[3]),
                                    0.0,
                                    std::ceil(pixels / camera.Grid[3]) - 1));
    result.Last[axis] =
        static_cast<int>(std::clamp(std::floor((last * 0.5 + 0.5) * pixels / camera.Grid[3]),
                                    0.0,
                                    std::ceil(pixels / camera.Grid[3]) - 1));
  }
  result.First[2] = static_cast<int>(
      std::max(ptrdiff_t{0}, std::ranges::upper_bound(edges, near) - edges.begin() - 1));
  result.Last[2] =
      static_cast<int>(std::min(static_cast<ptrdiff_t>(edges.size()) - 2,
                                std::ranges::upper_bound(edges, far) - edges.begin() - 1));
  result.Empty = result.First[2] > result.Last[2];
  return result;
}

bool Intersects(const Camera &camera,
                std::span<const double> edges,
                const Vector &position,
                double radius,
                const Bounds &cells) {
  const double near = edges[static_cast<size_t>(cells.First[2])];
  const double far = std::min(edges[static_cast<size_t>(cells.Last[2]) + 1], position[2] + radius);
  double distance = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    double low = near;
    double high = far;
    if (axis < 2) {
      const double first = static_cast<double>(cells.First[axis]) * camera.Grid[3];
      const double last = std::min(static_cast<double>(cells.Last[axis] + 1) * camera.Grid[3],
                                   static_cast<double>(camera.Grid[axis]));
      const double half = camera.Values[12 + axis];
      low = (first / camera.Grid[axis] * 2 - 1) * half;
      high = (last / camera.Grid[axis] * 2 - 1) * half;
      if (axis == 1) {
        const double old = low;
        low = -high;
        high = -old;
      }
      if (camera.Perspective) {
        const auto values = Products(low, high, near, far);
        low = values[0];
        high = values[1];
      }
    }
    const double delta = std::max({low - position[axis], position[axis] - high, 0.0});
    distance += delta * delta;
  }
  return distance <= radius * radius;
}
}
