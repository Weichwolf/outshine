#ifndef OUTSHINE_WORLD_ROUTE_H
#define OUTSHINE_WORLD_ROUTE_H

#include <cstddef>

#include "math/Vec3.h"

namespace outshine {

/// Value snapshot of a published route. Names identify routes within the current scenario;
/// publication may replace the route after a world rebuild. No source-format IDs are exposed.
struct RouteInfo {
  double LengthM = 0.0;    ///< Total centerline station in metres.
  size_t SegmentCount = 0; ///< Number of directed logical segments.
  bool Closed = false;     ///< Whether the final station meets the first.
};

/// Sample of a published route in the scenario's local east/up/south world frame.
/// PositionM shares the origin and axes of a local Camera pose. Forward and Up are unit,
/// perpendicular vectors; the caller owns this value and retains no Engine references.
struct RoutePose {
  Vec3 PositionM; ///< Local east/up/south position in metres relative to the scenario world origin.
  Vec3 Forward;   ///< Unit travel direction in the local world frame.
  Vec3 Up;        ///< Unit surface-up direction, perpendicular to Forward.
  double StationM = 0.0;   ///< Resolved centerline station in metres; closed end wraps to zero.
  double WidthM = 0.0;     ///< Usable surface width at this station, in metres.
  size_t SegmentIndex = 0; ///< Directed route segment ordinal, independent of the source format.
};

/// Contact on a currently published native road triangle at a route station and lateral offset.
/// Position and normal use the scenario's local east/up/south world frame. This owned value does
/// not retain a geometry or route publication; query again after a world update.
struct RouteContact {
  Vec3 PositionM;              ///< Surface point in local metres.
  Vec3 Normal;                 ///< Unit geometric triangle normal, pointing upward.
  double StationM = 0.0;       ///< Resolved route station in metres.
  double LateralOffsetM = 0.0; ///< Signed metres from centerline; positive means left.
  size_t SegmentIndex = 0;     ///< Logical directed route segment ordinal.
};

}

#endif
