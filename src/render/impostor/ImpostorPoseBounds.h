#ifndef OUTSHINE_RENDER_IMPOSTOR_IMPOSTORPOSEBOUNDS_H
#define OUTSHINE_RENDER_IMPOSTOR_IMPOSTORPOSEBOUNDS_H
#include "DistanceInterval.h"
#include "Lens.h"
#include "Viewing.h"
#include <array>
#include <expected>
#include <string>

namespace outshine::Render {
class ImpostorPoseBounds {
public:
  struct Error {
    Vec2 AxisUpperPx;
    double L1UpperPx = 0;
    bool DepthInside = false;
  };

  // Bounds the complete capture domain, not first-hit texels or GPU rounding.
  [[nodiscard]] static std::expected<ImpostorPoseBounds, std::string>
  From(Vec3 centre, Vec3 towardEye, double halfExtentM);

  // Camera is in capture-local coordinates. Support is the flat capture plane.
  // This grants no coverage, colour, depth, shadow or World replacement authority.
  [[nodiscard]] std::expected<Error, std::string> Project(const CameraBasis &camera,
                                                          const Lens &lens) const;

  // Camera and frame share World/ECEF coordinates; translation stays double precision.
  [[nodiscard]] std::expected<Error, std::string>
  Project(const CameraBasis &camera, const Lens &lens, const Mat4 &worldFromField) const;

private:
  ImpostorPoseBounds() = default;
  using Point = std::array<DistanceArithmetic::Interval, 3>;
  Vec3 Centre_;
  Vec3 Direction_;
  std::array<Point, 8> Corners_{};
};
}
#endif
