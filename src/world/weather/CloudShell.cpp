#include "CloudLayer.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace outshine {
namespace {

struct Roots {
  double NearM = 0.0;
  double FarM = 0.0;
  bool Hit = false;
};

Roots Sphere(const Vec3 &origin, const Vec3 &direction, double heightM, double radiusM) noexcept {
  const double b = Dot(origin, direction) + radiusM * direction[1];
  const double c =
      Dot(origin, origin) + 2.0 * radiusM * origin[1] - heightM * (2.0 * radiusM + heightM);
  const double discriminant = b * b - c;
  if (discriminant < 0.0) { return {}; }
  const double root = std::sqrt(discriminant);
  return {.NearM = -b - root, .FarM = -b + root, .Hit = true};
}

}

Vec3 CloudWindFrom(const CloudWind &wind) noexcept {
  const double bearing = std::remainder(wind.FromDeg, 360.0) * (std::numbers::pi / 180.0);
  return {{-std::sin(bearing) * wind.SpeedMs, 0.0, std::cos(bearing) * wind.SpeedMs}};
}

double CloudAltitude(const Vec3 &positionM, double radiusM) noexcept {
  const double relative = Dot(positionM, positionM) + 2.0 * radiusM * positionM[1];
  return relative / (std::sqrt(relative + radiusM * radiusM) + radiusM);
}

CloudSegments
CloudShell(const CloudLayer &layer, const Vec3 &originM, const Vec3 &direction) noexcept {
  const Roots outer = Sphere(originM, direction, layer.BaseM + layer.ThicknessM, layer.RadiusM);
  if (!outer.Hit || outer.FarM <= 0.0) { return {}; }
  const Roots inner = Sphere(originM, direction, layer.BaseM, layer.RadiusM);
  const Roots ground = Sphere(originM, direction, 0.0, layer.RadiusM);
  const double start = std::max(outer.NearM, 0.0);
  double stop = outer.FarM;
  const bool inward = Dot(originM, direction) + layer.RadiusM * direction[1] < 0.0;
  if (ground.Hit && inward && ground.NearM >= 0.0) { stop = std::min(stop, ground.NearM); }
  if (stop <= start) { return {}; }
  if (!inner.Hit || inner.FarM <= start || inner.NearM >= stop) {
    return {{{.BeginM = start, .EndM = stop}, {}}};
  }
  const double split = std::clamp(inner.NearM, start, stop);
  const double secondStart = std::max(inner.FarM, start);
  CloudSegments result = {{{.BeginM = start, .EndM = split},
                           {.BeginM = secondStart, .EndM = std::max(secondStart, stop)}}};
  if (result[0].BeginM == result[0].EndM && result[1].EndM > result[1].BeginM) {
    result[0] = result[1];
    result[1] = {};
  }
  for (CloudSegment &segment : result) {
    if (segment.EndM <= segment.BeginM) { segment = {}; }
  }
  return result;
}

}
