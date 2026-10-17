#ifndef OUTSHINE_GENERATORS_FLORA_PLANTBLADESURFACE_H
#define OUTSHINE_GENERATORS_FLORA_PLANTBLADESURFACE_H

#include "PlantSurface.h"
#include <cstddef>
#include <expected>
#include <string_view>

namespace outshine::Generators {
struct PlantBlade {
  double LengthM = 0, WidthM = 0, Widest = 0;
  int Lobes = 0;
  double LobeDepth = 0, BaseFill = 0, BendRad = 0, Fold = 0;
  int FoldPower = 2;
  double TwistRad = 0, PetioleM = 0;
};

struct PlantBladeSurfaceDemand {
  size_t Segments = 0, Across = 0;
  size_t MaximumVertices = 0, MaximumTriangles = 0;
};

[[nodiscard]] std::expected<PlantSurface, std::string_view>
BuildPlantBladeSurface(const PlantBlade &blade, PlantBladeSurfaceDemand demand);
[[nodiscard]] std::expected<PlantSurfaceSize, std::string_view>
MeasurePlantBladeSurface(const PlantBlade &blade, PlantBladeSurfaceDemand demand);
}
#endif
