#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_TRANSPORTROADPAGEMESHING_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_TRANSPORTROADPAGEMESHING_H

#include "RoadPageGeometry.h"
#include "TransportPlan.h"
#include <expected>
#include <span>
#include <stop_token>
#include <string_view>

namespace outshine::Generators {
// Borrows a validated complete plan during meshing; the result owns its buffers.
[[nodiscard]] std::expected<RoadPageGeometry, std::string_view>
MeshTransportRoadPage(const RoadMesher &mesher,
                      const TransportPlan &plan,
                      std::span<const RoadPagePartAppearance> parts,
                      const std::stop_token &stop = {});
}
#endif
