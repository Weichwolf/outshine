#include "ViewPath.h"
#include "Box.h"
#include "DistanceInterval.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace outshine {
namespace {
Vec3 ClosestSeparatingAxis(const Box &bounds, const ViewPath &path) {
  const Vec3 advance = path.End - path.Start;
  const Vec3 minimum = bounds.Min - path.Start;
  const Vec3 maximum = bounds.Max - path.Start;
  const auto finite = [](double value) { return std::isfinite(value); };
  if (!std::ranges::all_of(advance, finite) || !std::ranges::all_of(minimum, finite) ||
      !std::ranges::all_of(maximum, finite)) {
    return {};
  }
  std::array<double, 8> parameters{0, 1};
  size_t count = 2;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (advance[axis] == 0) { continue; }
    parameters[count++] = std::clamp(minimum[axis] / advance[axis], 0.0, 1.0);
    parameters[count++] = std::clamp(maximum[axis] / advance[axis], 0.0, 1.0);
  }
  std::sort(parameters.begin(), parameters.begin() + static_cast<std::ptrdiff_t>(count));
  Vec3 best;
  double squared = std::numeric_limits<double>::infinity();
  for (size_t at = 1; at < count; ++at) {
    const double first = parameters[at - 1];
    const double last = parameters[at];
    const Vec3 middle = advance * ((first + last) * 0.5);
    double sum = 0;
    double weight = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      const double side = std::clamp(middle[axis], minimum[axis], maximum[axis]);
      if (side == middle[axis]) { continue; }
      sum += side * advance[axis];
      weight += advance[axis] * advance[axis];
    }
    const Vec3 eye = advance * (weight > 0 ? std::clamp(sum / weight, first, last) : first);
    const Vec3 closest{{std::clamp(eye[0], minimum[0], maximum[0]),
                        std::clamp(eye[1], minimum[1], maximum[1]),
                        std::clamp(eye[2], minimum[2], maximum[2])}};
    const Vec3 separation = eye - closest;
    const double candidate = Dot(separation, separation);
    if (candidate < squared) {
      best = separation;
      squared = candidate;
    }
  }
  return best;
}

double PathDistanceBelow(const Box &bounds, const ViewPath &path) {
  using namespace DistanceArithmetic;
  Vec3 normal = ClosestSeparatingAxis(bounds, path);
  if (!Normalise(normal)) { return 0; }
  std::array<Interval, 3> components{};
  Interval start{};
  Interval finish{};
  const auto exact = [](double value) { return Interval{.Lower = value, .Upper = value}; };
  for (size_t axis = 0; axis < 3; ++axis) {
    const double support = normal[axis] < 0 ? bounds.Min[axis] : bounds.Max[axis];
    const Interval plane = exact(support);
    components[axis] = exact(normal[axis]);
    start = Add(start, Multiply(components[axis], Subtract(exact(path.Start[axis]), plane)));
    finish = Add(finish, Multiply(components[axis], Subtract(exact(path.End[axis]), plane)));
  }
  const double separation = std::min(start.Lower, finish.Lower);
  return separation > 0 ? std::nextafter(separation / UpperNorm(components), 0.0) : 0;
}

bool ValidPath(const Box &bounds, const ViewPath &path) {
  const auto finite = [](double value) { return std::isfinite(value); };
  return std::ranges::all_of(path.Start, finite) && std::ranges::all_of(path.End, finite) &&
         finite(path.RadiusM) && path.RadiusM >= 0 && std::ranges::all_of(bounds.Min, finite) &&
         std::ranges::all_of(bounds.Max, finite) && bounds.Holds(bounds.Max);
}
}

double ViewPath::DistanceBelow(const Box &bounds) const noexcept {
  if (!ValidPath(bounds, *this)) { return 0; }
  return std::max(0.0, std::nextafter(PathDistanceBelow(bounds, *this) - RadiusM, 0.0));
}

ViewPath ViewPathInEcef(const ViewPath &scene, const TangentFrame &frame) {
  const auto ecef = [&frame](const Vec3 &position) {
    return frame.ToEcefPosition({.EastM = position[0], .NorthM = -position[2], .UpM = position[1]});
  };
  const Vec3 start = ecef(scene.Start);
  const Vec3 end = ecef(scene.End);
  const double guard = (Length(start) + Length(end)) * (8 * std::numeric_limits<double>::epsilon());
  return {.Start = start,
          .End = end,
          .RadiusM =
              std::nextafter(scene.RadiusM + guard, std::numeric_limits<double>::infinity())};
}

}
