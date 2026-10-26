#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANAREAPRISMS_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANAREAPRISMS_H

#include "UrbanArea.h"
#include "Mat4.h"
#include <PrismGrid.h>
#include <cstddef>
#include <expected>
#include <mdspan>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators::Osm {
using UrbanGroundHeights = std::mdspan<const std::optional<double>, std::dextents<size_t, 2>>;

struct UrbanPrismInput {
  UrbanPatchRange Range;
  UrbanGroundHeights GroundAslM;
};

struct UrbanPrismPlacement {
  Mat4 EcefFromLocal;
  double MaxCornerErrorM = 0;
};

struct UrbanPrisms {
  PrismGrid Geometry;
  std::vector<UrbanPrismPlacement> Placements;
};

enum class UrbanPrismError { InvalidRequest, MissingTerrain, OverBudget, Unrepresentable };

[[nodiscard]] std::expected<UrbanPrisms, UrbanPrismError> PrepareUrbanPrisms(
    const UrbanArea &area, std::span<const UrbanPrismInput> patches, size_t samplesMost);
}
#endif
