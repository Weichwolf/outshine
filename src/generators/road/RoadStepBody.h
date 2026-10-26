#ifndef OUTSHINE_GENERATORS_ROAD_ROADSTEPBODY_H
#define OUTSHINE_GENERATORS_ROAD_ROADSTEPBODY_H

#include "RoadMesher.h"
#include "SurfaceBody.h"
#include <cstdint>
#include <expected>
#include <span>

namespace outshine::Generators {
enum class RoadStepError : uint8_t { InvalidInput, UnsupportedGeometry, BodyFailure };

[[nodiscard]] std::expected<SurfaceBody, RoadStepError>
BuildRoadStepBody(std::span<const RoadStation> stations, const RoadSweep &surface);

[[nodiscard]] bool
AppendRoadStepBody(const SurfaceBody &body, const Vec3f &colour, RoadMeshBuffers &mesh);
}
#endif
