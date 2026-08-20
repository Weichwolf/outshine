#ifndef OUTSHINE_WORLD_WAYFINDING_H
#define OUTSHINE_WORLD_WAYFINDING_H

#include <cstddef>
#include <string>
#include <vector>

namespace outshine::World {

inline constexpr size_t kMaxRouteLegs = 65536;

struct Waypoint {
  double LatDeg = 0.0;
  double LonDeg = 0.0;
};

struct Leg {
  Waypoint At;
  double AlongM = 0.0;
  double HalfWidthM = 0.0;
  long WayId = 0;
};

struct Route {
  bool Found = false;
  double LengthM = 0.0;
  std::vector<Leg> Legs;
  std::string Error;
};

[[nodiscard]] Route Plan(const Waypoint &from, const Waypoint &to);

} // namespace outshine::World

#endif
