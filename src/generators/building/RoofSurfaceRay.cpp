#include "RoofSurface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators {
namespace {

constexpr double kSawtoothRidgeShare = 0.85;

Vec3 BoxRay(const BuildingShape &shape, const Vec3 &value, bool point) {
  const EastNorth at{.EastM = value[0], .NorthM = value[1]};
  if (point) {
    const auto boxed = shape.ToBox(at);
    return {{boxed.U, boxed.V, value[2]}};
  }
  const auto v = shape.AxisV();
  return {{at.EastM * shape.AxisU.EastM + at.NorthM * shape.AxisU.NorthM,
           at.EastM * v.EastM + at.NorthM * v.NorthM,
           value[2]}};
}

bool Restrict(const BuildingShape &shape,
              const Vec3 &origin,
              const Vec3 &direction,
              double &minimum,
              double &maximum) {
  const Vec3 low{{-shape.HalfUm, -shape.HalfVm, 0.0}};
  const Vec3 high{{shape.HalfUm, shape.HalfVm, shape.RiseM}};
  for (size_t axis = 0; axis < 3; ++axis) {
    if (direction[axis] == 0.0) {
      if (origin[axis] < low[axis] || origin[axis] > high[axis]) { return false; }
      continue;
    }
    const double a = (low[axis] - origin[axis]) / direction[axis];
    const double b = (high[axis] - origin[axis]) / direction[axis];
    minimum = std::max(minimum, std::min(a, b));
    maximum = std::min(maximum, std::max(a, b));
    if (minimum > maximum) { return false; }
  }
  return std::isfinite(minimum) && std::isfinite(maximum);
}

Vec3 NormalAt(const BuildingShape &shape, const EastNorth &point) {
  const auto box = shape.ToBox(point);
  const double u = box.U;
  const double v = box.V;
  const double hu = shape.HalfUm;
  const double hv = shape.HalfVm;
  double du = 0.0;
  double dv = 0.0;
  switch (shape.Roof) {
    case RoofKind::Flat: break;
    case RoofKind::Gable: dv = -std::copysign(shape.RiseM / hv, v); break;
    case RoofKind::Hip:
      if (hv - std::abs(v) <= hu - std::abs(u)) {
        dv = -std::copysign(shape.RiseM / hv, v);
      } else {
        du = -std::copysign(shape.RiseM / hv, u);
      }
      break;
    case RoofKind::Shed: dv = shape.RiseM / (2.0 * hv); break;
    case RoofKind::Mansard: {
      const double b = shape.BreakFracV * hv;
      const double slope = std::abs(v) >= b
                               ? shape.BreakRiseM / std::max(hv - b, 1.0e-3)
                               : (shape.RiseM - shape.BreakRiseM) / std::max(b, 1.0e-3);
      dv = -std::copysign(slope, v);
      break;
    }
    case RoofKind::Sawtooth: {
      const double period = std::max(shape.PeriodM, 1.0);
      double at = std::fmod(u + hu, period);
      if (at < 0.0) { at += period; }
      du = at < kSawtoothRidgeShare * period
               ? shape.RiseM / (kSawtoothRidgeShare * period)
               : -shape.RiseM / ((1.0 - kSawtoothRidgeShare) * period);
      break;
    }
    case RoofKind::Dome: {
      const double remainder = 1.0 - (u / hu) * (u / hu) - (v / hv) * (v / hv);
      if (remainder <= 0.0) { break; }
      const double scale = shape.RiseM / std::sqrt(remainder);
      du = -scale * u / (hu * hu);
      dv = -scale * v / (hv * hv);
      break;
    }
  }
  const auto vAxis = shape.AxisV();
  Vec3 normal{{-du * shape.AxisU.EastM - dv * vAxis.EastM,
               -du * shape.AxisU.NorthM - dv * vAxis.NorthM,
               1.0}};
  return normal * (1.0 / std::hypot(normal[0], normal[1], normal[2]));
}

std::optional<RoofSurface::Hit> HitAt(const RoofSurface &roof,
                                      const BuildingShape &shape,
                                      const Vec3 &origin,
                                      const Vec3 &direction,
                                      double along,
                                      double minimum,
                                      double maximum) {
  if (along < minimum || along > maximum || !std::isfinite(along)) { return std::nullopt; }
  const EastNorth point{.EastM = origin[0] + along * direction[0],
                        .NorthM = origin[1] + along * direction[1]};
  if (!roof.Contains(point)) { return std::nullopt; }
  return RoofSurface::Hit{.Along = along, .Normal = NormalAt(shape, point)};
}

void DomeCuts(const BuildingShape &shape,
              const Vec3 &origin,
              const Vec3 &direction,
              double minimum,
              double maximum,
              std::vector<double> &cuts) {
  cuts.clear();
  const Vec3 scale{{shape.HalfUm, shape.HalfVm, shape.RiseM}};
  double a = 0.0;
  double b = 0.0;
  double c = -1.0;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double o = origin[axis] / scale[axis];
    const double d = direction[axis] / scale[axis];
    a += d * d;
    b += 2.0 * o * d;
    c += o * o;
  }
  const double discriminant = b * b - 4.0 * a * c;
  if (a > 0.0 && discriminant >= 0.0) {
    const double q = -0.5 * (b + std::copysign(std::sqrt(discriminant), b));
    const std::array roots{q / a, q == 0.0 ? -b / (2.0 * a) : c / q};
    for (const double along : roots) {
      if (along >= minimum && along <= maximum && origin[2] + along * direction[2] >= 0.0) {
        cuts.push_back(along);
      }
    }
  }
  if (direction[2] != 0.0) {
    const double along = -origin[2] / direction[2];
    const double u = (origin[0] + along * direction[0]) / shape.HalfUm;
    const double v = (origin[1] + along * direction[1]) / shape.HalfVm;
    if (along >= minimum && along <= maximum && u * u + v * v >= 1.0) { cuts.push_back(along); }
  }
  std::ranges::sort(cuts);
}

void SawtoothCuts(const BuildingShape &shape,
                  const Vec3 &origin,
                  const Vec3 &direction,
                  double minimum,
                  double maximum,
                  std::vector<double> &cuts) {
  cuts.clear();
  if (direction[0] == 0.0) { return; }
  const double period = std::max(shape.PeriodM, 1.0);
  const double a = origin[0] + minimum * direction[0] + shape.HalfUm;
  const double b = origin[0] + maximum * direction[0] + shape.HalfUm;
  const double first = std::floor(std::min(a, b) / period);
  const double last = std::ceil(std::max(a, b) / period);
  const auto periods = static_cast<size_t>(last - first);
  for (size_t at = 0; at <= periods; ++at) {
    const double ridge = first + static_cast<double>(at);
    for (const double share : {0.0, kSawtoothRidgeShare}) {
      const double along = ((ridge + share) * period - shape.HalfUm - origin[0]) / direction[0];
      if (along > minimum && along < maximum) { cuts.push_back(along); }
    }
  }
}

struct QueryRay {
  Vec3 Origin;
  Vec3 Direction;
};

bool ValidQuery(const QueryRay &ray, double minimum, double maximum) {
  const auto finite = [](const Vec3 &value) {
    return std::ranges::all_of(value, [](double coordinate) { return std::isfinite(coordinate); });
  };
  return minimum >= 0.0 && maximum >= minimum && std::isfinite(minimum) && !std::isnan(maximum) &&
         finite(ray.Origin) && finite(ray.Direction) &&
         std::hypot(ray.Direction[0], ray.Direction[1], ray.Direction[2]) > 0.0;
}

std::optional<RoofSurface::Hit> FirstHit(const RoofSurface &roof,
                                         const BuildingShape &shape,
                                         const QueryRay &ray,
                                         double minimum,
                                         double maximum,
                                         std::span<const double> cuts) {
  for (const double along : cuts) {
    if (auto hit = HitAt(roof, shape, ray.Origin, ray.Direction, along, minimum, maximum)) {
      return hit;
    }
  }
  return std::nullopt;
}

}

std::optional<RoofSurface::Hit> RoofSurface::Trace(const Vec3 &origin,
                                                   const Vec3 &direction,
                                                   double minimum,
                                                   double maximum,
                                                   std::vector<double> &cuts) const {
  if (!ValidQuery({.Origin = origin, .Direction = direction}, minimum, maximum)) {
    return std::nullopt;
  }
  if (Shape_.Roof == RoofKind::Flat || Shape_.RiseM == 0.0) {
    if (direction[2] == 0.0) { return std::nullopt; }
    return HitAt(*this, Shape_, origin, direction, -origin[2] / direction[2], minimum, maximum);
  }
  const Vec3 start = BoxRay(Shape_, origin, true);
  const Vec3 ray = BoxRay(Shape_, direction, false);
  if (!Restrict(Shape_, start, ray, minimum, maximum)) { return std::nullopt; }
  if (Shape_.Roof == RoofKind::Dome && Shape_.RiseM > 0.0) {
    DomeCuts(Shape_, start, ray, minimum, maximum, cuts);
    return FirstHit(
        *this, Shape_, {.Origin = origin, .Direction = direction}, minimum, maximum, cuts);
  }
  if (Shape_.Roof == RoofKind::Sawtooth) {
    SawtoothCuts(Shape_, start, ray, minimum, maximum, cuts);
  } else {
    BreaksAlong(
        {.EastM = origin[0] + minimum * direction[0], .NorthM = origin[1] + minimum * direction[1]},
        {.EastM = origin[0] + maximum * direction[0], .NorthM = origin[1] + maximum * direction[1]},
        cuts);
    for (double &cut : cuts) { cut = minimum + cut * (maximum - minimum); }
  }
  cuts.push_back(minimum);
  cuts.push_back(maximum);
  std::ranges::sort(cuts);
  const auto delta = [&](double along) {
    return origin[2] + along * direction[2] -
           HeightAt({.EastM = origin[0] + along * direction[0],
                     .NorthM = origin[1] + along * direction[1]});
  };
  for (size_t at = 1; at < cuts.size(); ++at) {
    const double a = cuts[at - 1];
    const double b = cuts[at];
    const double da = delta(a);
    const double db = delta(b);
    if ((da > 0.0 && db > 0.0) || (da < 0.0 && db < 0.0)) { continue; }
    const double along = da == db ? a : a + (b - a) * da / (da - db);
    if (auto hit = HitAt(*this, Shape_, origin, direction, along, minimum, maximum)) { return hit; }
  }
  return std::nullopt;
}

}
