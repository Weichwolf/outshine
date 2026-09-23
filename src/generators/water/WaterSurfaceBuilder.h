#ifndef OUTSHINE_GENERATORS_WATER_WATERSURFACEBUILDER_H
#define OUTSHINE_GENERATORS_WATER_WATERSURFACEBUILDER_H

#include "WaterField.h"
#include "scene/Geometry.h"

#include <cstddef>
#include <expected>
#include <span>
#include <string>

namespace outshine {
class TangentFrame;
}

namespace outshine::Generators {

struct WaterSurfaceMetrics {
  size_t Laid = 0;
  size_t RefusedTopology = 0;
  size_t Triangles = 0;
};

[[nodiscard]] std::expected<WaterSurfaceMetrics, std::string>
AppendWaterSurfaceGeometry(Geometry &geometry,
                           MaterialInstance material,
                           const outshine::Ground::WaterField &water,
                           std::span<const double> geographicPoints,
                           const TangentFrame &frame);

}
#endif
