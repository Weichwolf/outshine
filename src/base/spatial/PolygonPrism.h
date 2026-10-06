#ifndef OUTSHINE_BASE_SPATIAL_POLYGONPRISM_H
#define OUTSHINE_BASE_SPATIAL_POLYGONPRISM_H

#include "math/Box.h"
#include "math/Vec2.h"
#include "math/Vec3.h"

#include <cstddef>
#include <optional>
#include <span>

namespace outshine {

struct PolygonPrism {
  struct Hit {
    double Along = 0.0;
    Vec3 Normal;
    size_t Face = 0;
  };

  std::span<const Vec2> Ring;
  std::span<const std::span<const Vec2>> Holes;
  double Bottom = 0.0, Top = 0.0;

  [[nodiscard]] Box Bounds() const noexcept;
  [[nodiscard]] std::optional<Hit>
  Trace(const Vec3 &origin, const Vec3 &direction, double minimum, double maximum) const noexcept;
};

}
#endif
