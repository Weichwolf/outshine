#ifndef OUTSHINE_GENERATORS_WATER_WATERSURFACEBUILDER_H
#define OUTSHINE_GENERATORS_WATER_WATERSURFACEBUILDER_H

#include "WaterAsset.h"
#include "Earth.h"
#include "scene/Geometry.h"
#include "scene/Material.h"

#include <cstddef>
#include <expected>
#include <optional>
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

[[nodiscard]] Material WaterSurfaceMaterial() noexcept;

[[nodiscard]] std::optional<double>
WaterSurfaceUpAt(const WaterAsset &water, const TangentFrame &frame, LongitudeLatitude at) noexcept;

[[nodiscard]] std::expected<WaterSurfaceMetrics, std::string>
AppendWaterSurfaceGeometry(Geometry &geometry,
                           MaterialInstance material,
                           const WaterAsset &water,
                           const TangentFrame &frame);

}
#endif
