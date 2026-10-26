#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_TRANSPORTPORTFRAME_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_TRANSPORTPORTFRAME_H

#include "TangentFrame.h"
#include "TransportPlan.h"
#include <expected>
#include <string_view>

namespace outshine::Generators {
[[nodiscard]] std::expected<RoadPort, std::string_view>
TransportPortInFrame(const TransportPort &port, const TangentFrame &frame, EastNorth interior);
}
#endif
