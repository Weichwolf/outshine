#ifndef OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORVIEWFRAME_H
#define OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORVIEWFRAME_H

#include "Vec3.h"
#include <cmath>
#include <optional>

namespace outshine {

struct ImpostorViewFrame {
  Vec3 Right;
  Vec3 Up;

  [[nodiscard]] static std::optional<ImpostorViewFrame> From(const Vec3 &towardEye) noexcept {
    constexpr double tolerance = 1e-12;
    const double norm = Dot(towardEye, towardEye);
    if (!std::isfinite(norm) || std::abs(norm - 1) > tolerance) { return std::nullopt; }
    Vec3 right{{towardEye[2], 0, -towardEye[0]}};
    if (towardEye[1] == 0) { return ImpostorViewFrame{.Right = right, .Up = {{0, 1, 0}}}; }
    if (!Normalise(right)) { right = {{1, 0, 0}}; }
    return ImpostorViewFrame{.Right = right, .Up = Cross(towardEye, right)};
  }
};

}
#endif
