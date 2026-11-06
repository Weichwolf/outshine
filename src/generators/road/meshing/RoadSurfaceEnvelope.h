#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_ROADSURFACEENVELOPE_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_ROADSURFACEENVELOPE_H

#include "RoadMesher.h"
#include "RibbonSampling.h"
#include <cstddef>
#include <expected>
#include <string_view>

namespace outshine::Generators {
struct RoadSurfaceEnvelopeStats {
  double PositionBoundM = 0;
  size_t OriginalSpans = 0;
  size_t EnvelopeSpans = 0;
};

// Finite canonical surface reference. Longitudinal stations and complete run ends are retained.
[[nodiscard]] std::expected<RoadSurfaceEnvelopeStats, std::string_view>
AppendRoadSurfaceEnvelope(const ReferenceLine &line,
                          const RibbonSamplingPlan &sampling,
                          const RoadSweep &surface,
                          double errorM,
                          RoadMeshBuffers &mesh);
}
#endif
