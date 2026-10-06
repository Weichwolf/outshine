#include "PolygonPrism.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

namespace outshine {
namespace {

double Cross(const Vec2 &a, const Vec2 &b) noexcept {
  return a[0] * b[1] - a[1] * b[0];
}

bool Inside(std::span<const Vec2> ring, const Vec2 &point) noexcept {
  bool inside = false;
  for (size_t at = 0; at < ring.size(); ++at) {
    const Vec2 &a = ring[at];
    const Vec2 &b = ring[(at + 1) % ring.size()];
    const Vec2 edge = b - a;
    const Vec2 offset = point - a;
    const double along = offset[0] * edge[0] + offset[1] * edge[1];
    const double lengthSquared = edge[0] * edge[0] + edge[1] * edge[1];
    if (lengthSquared > 0.0 && Cross(edge, offset) == 0.0 && along >= 0.0 &&
        along <= lengthSquared) {
      return true;
    }
    if ((a[1] > point[1]) != (b[1] > point[1]) &&
        point[0] < a[0] + (point[1] - a[1]) * edge[0] / edge[1]) {
      inside = !inside;
    }
  }
  return inside;
}

void TraceWalls(const PolygonPrism &prism,
                std::span<const Vec2> ring,
                bool hole,
                const Vec3 &origin,
                const Vec3 &direction,
                size_t &face,
                std::optional<PolygonPrism::Hit> &hit,
                double minimum,
                double &maximum) noexcept {
  double area = 0.0;
  for (size_t at = 0; at < ring.size(); ++at) {
    area += Cross(ring[at], ring[(at + 1) % ring.size()]);
  }
  const double orientation = (area >= 0.0) != hole ? 1.0 : -1.0;
  const Vec2 start{{origin[0], origin[1]}};
  const Vec2 ray{{direction[0], direction[1]}};
  for (size_t at = 0; at < ring.size(); ++at, ++face) {
    const Vec2 &a = ring[at];
    const Vec2 edge = ring[(at + 1) % ring.size()] - a;
    const double divisor = Cross(ray, edge);
    if (divisor == 0.0) { continue; }
    const Vec2 offset = a - start;
    const double distance = Cross(offset, edge) / divisor;
    const double along = Cross(offset, ray) / divisor;
    const double z = origin[2] + distance * direction[2];
    if (distance < minimum || distance > maximum || along < 0.0 || along > 1.0 ||
        z < prism.Bottom || z > prism.Top || !std::isfinite(distance)) {
      continue;
    }
    const double length = std::hypot(edge[0], edge[1]);
    if (length == 0.0) { continue; }
    hit = PolygonPrism::Hit{
        .Along = distance,
        .Normal = {{orientation * edge[1] / length, -orientation * edge[0] / length, 0.0}},
        .Face = face};
    maximum = distance;
  }
}

}

Box PolygonPrism::Bounds() const noexcept {
  Box bounds;
  for (const Vec2 &point : Ring) {
    bounds.Cover(Vec3{{point[0], point[1], Bottom}});
    bounds.Cover(Vec3{{point[0], point[1], Top}});
  }
  return bounds;
}

std::optional<PolygonPrism::Hit> PolygonPrism::Trace(const Vec3 &origin,
                                                     const Vec3 &direction,
                                                     double minimum,
                                                     double maximum) const noexcept {
  if (Ring.size() < 3 || !(Bottom < Top) || minimum < 0.0 || minimum > maximum ||
      !std::isfinite(minimum) || std::isnan(maximum)) {
    return std::nullopt;
  }
  std::optional<Hit> hit;
  if (direction[2] != 0.0) {
    for (size_t face = 0; face < 2; ++face) {
      const double height = face == 0 ? Bottom : Top;
      const double along = (height - origin[2]) / direction[2];
      if (along < minimum || along > maximum || !std::isfinite(along)) { continue; }
      const Vec2 point{{origin[0] + along * direction[0], origin[1] + along * direction[1]}};
      if (!Inside(Ring, point) ||
          std::ranges::any_of(Holes, [&](auto hole) { return Inside(hole, point); })) {
        continue;
      }
      hit = Hit{.Along = along, .Normal = {{0.0, 0.0, face == 0 ? -1.0 : 1.0}}, .Face = face};
      maximum = along;
    }
  }
  size_t face = 2;
  TraceWalls(*this, Ring, false, origin, direction, face, hit, minimum, maximum);
  for (const auto hole : Holes) {
    TraceWalls(*this, hole, true, origin, direction, face, hit, minimum, maximum);
  }
  return hit;
}

}
