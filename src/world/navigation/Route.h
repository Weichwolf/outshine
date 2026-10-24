#ifndef OUTSHINE_WORLD_ROUTE_H
#define OUTSHINE_WORLD_ROUTE_H

#include <cstddef>

#include "Vec3.h"

namespace outshine {

struct RouteInfo {
  double LengthM = 0.0;
  size_t SegmentCount = 0;
  bool Closed = false;
};

struct RoutePose {
  Vec3 PositionM;
  Vec3 Forward;
  Vec3 Up;
  double StationM = 0.0;
  double WidthM = 0.0;
  size_t SegmentIndex = 0;
};

struct RouteContact {
  Vec3 PositionM;
  Vec3 Normal;
  double StationM = 0.0;
  double LateralOffsetM = 0.0;
  size_t SegmentIndex = 0;
};

}

#endif
