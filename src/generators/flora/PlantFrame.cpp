#include "PlantFrame.h"
#include "math/Vec3.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace outshine::Generators {
namespace {
constexpr double kLeastCrossLength = 1e-8;
}

Vec3 PlantRotate(const std::array<double, 9> &frame, const Vec3 &point) noexcept {
  Vec3 result;
  for (size_t row = 0; row < 3; ++row) {
    for (size_t column = 0; column < 3; ++column) {
      result[row] += frame[row * 3 + column] * point[column];
    }
  }
  return result;
}

std::optional<std::array<double, 9>> PlantNormalFrame(Vec3 direction, const Vec3 &hint) noexcept {
  const double length = Length(direction);
  if (!(length > 0) || !std::isfinite(length)) { return std::nullopt; }
  direction = direction * (1.0 / length);
  Vec3 lateral = Cross(direction, hint);
  if (Length(lateral) < kLeastCrossLength) {
    size_t least = 0;
    for (size_t at = 1; at < 3; ++at) {
      if (std::abs(direction[at]) < std::abs(direction[least])) { least = at; }
    }
    Vec3 fallback;
    fallback[least] = 1;
    lateral = Cross(direction, fallback);
  }
  lateral = lateral * (1.0 / Length(lateral));
  const Vec3 normal = Cross(lateral, direction);
  return std::array{lateral[0],
                    direction[0],
                    normal[0],
                    lateral[1],
                    direction[1],
                    normal[1],
                    lateral[2],
                    direction[2],
                    normal[2]};
}

std::array<double, 9> PlantComposeFrames(const std::array<double, 9> &outer,
                                         const std::array<double, 9> &inner) noexcept {
  std::array<double, 9> result{};
  for (size_t row = 0; row < 3; ++row) {
    for (size_t column = 0; column < 3; ++column) {
      for (size_t middle = 0; middle < 3; ++middle) {
        result[row * 3 + column] += outer[row * 3 + middle] * inner[middle * 3 + column];
      }
    }
  }
  return result;
}
}
