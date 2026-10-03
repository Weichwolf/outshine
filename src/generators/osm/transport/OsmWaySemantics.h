#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMWAYSEMANTICS_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMWAYSEMANTICS_H

#include <cstdint>
#include <expected>

#include "OsmTransportBuilder.h"

namespace outshine::Generators::Osm {

enum class OsmWayTravel : uint8_t { Forward, Reverse, Both };

struct OsmWaySemantics {
  bool TransportTagged = false;
  uint8_t Modes = 0;
  World::TransportFacility Facility = World::TransportFacility::Unknown;
  World::TransportSurface Surface = World::TransportSurface::Unknown;
  double WidthM = 0.0;
  uint8_t LaneCount = 0;
  int32_t Layer = 0;
  bool Bridge = false;
  bool Tunnel = false;
  World::TransportAccess Access = World::TransportAccess::Public;
  OsmWayTravel Travel = OsmWayTravel::Both;
};

[[nodiscard]] std::expected<OsmWaySemantics, TransportBuildErrorCode>
DescribeOsmWay(const Way &way);

}

#endif
