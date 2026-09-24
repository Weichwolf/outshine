#ifndef OUTSHINE_WORLD_NAVIGATION_OSMWAYSEMANTICS_H
#define OUTSHINE_WORLD_NAVIGATION_OSMWAYSEMANTICS_H

#include <cstdint>
#include <expected>

#include "TransportTopology.h"

namespace outshine::World {

enum class OsmWayTravel : uint8_t { Forward, Reverse, Both };

struct OsmWaySemantics {
  bool TransportTagged = false;
  uint8_t Modes = 0;
  TransportFacility Facility = TransportFacility::Unknown;
  TransportSurface Surface = TransportSurface::Unknown;
  double WidthM = 0.0;
  uint8_t LaneCount = 0;
  int32_t Layer = 0;
  bool Bridge = false;
  bool Tunnel = false;
  TransportAccess Access = TransportAccess::Public;
  OsmWayTravel Travel = OsmWayTravel::Both;
};

[[nodiscard]] std::expected<OsmWaySemantics, TransportBuildErrorCode>
DescribeOsmWay(const Data::OsmWay &way);

}

#endif
