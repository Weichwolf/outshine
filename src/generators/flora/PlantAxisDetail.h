#ifndef OUTSHINE_GENERATORS_FLORA_PLANTAXISDETAIL_H
#define OUTSHINE_GENERATORS_FLORA_PLANTAXISDETAIL_H

#include "PlantPlan.h"
#include <cstddef>
#include <expected>
#include <string_view>

namespace outshine::Generators {
struct PlantAxisDetail {
  size_t Segments = 0, Sides = 0;
  double ParametricErrorM = 0;
};

[[nodiscard]] std::expected<double, std::string_view>
BoundPlantAxisParametricError(const PlantAxis &axis, size_t segments, size_t sides);
[[nodiscard]] std::expected<PlantAxisDetail, std::string_view>
SelectPlantAxisDetail(const PlantAxis &axis, double maximumParametricErrorM);
}
#endif
