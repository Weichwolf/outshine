#ifndef OUTSHINE_GENERATORS_FLORA_PLANTAXISSURFACE_H
#define OUTSHINE_GENERATORS_FLORA_PLANTAXISSURFACE_H

#include "PlantPlan.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct PlantAxisSurfaceDemand {
  uint64_t Axis = 0;
  size_t Segments = 0, Sides = 0;
  size_t MaximumVertices = 0, MaximumTriangles = 0;
};

struct PlantAxisSurface {
  std::vector<float> Positions, Normals;
  std::vector<uint32_t> Triangles;
};

[[nodiscard]] std::expected<PlantAxisSurface, std::string_view>
BuildPlantAxisSurface(const PlantPlan &plan, PlantAxisSurfaceDemand demand);
}
#endif
