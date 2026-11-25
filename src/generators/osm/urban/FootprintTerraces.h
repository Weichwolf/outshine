#ifndef OUTSHINE_GENERATORS_OSM_URBAN_FOOTPRINTTERRACES_H
#define OUTSHINE_GENERATORS_OSM_URBAN_FOOTPRINTTERRACES_H

#include "PolygonTriangulation.h"
#include "UrbanAreaPrisms.h"
#include <cstddef>
#include <expected>
#include <span>
#include <stop_token>
#include <string>

namespace outshine::Generators::Osm {
inline constexpr unsigned kTerracePatchSpanM = 256;

[[nodiscard]] constexpr bool ValidTerraceSpacing(unsigned spacingM) noexcept {
  return spacingM != 0 && kTerracePatchSpanM % spacingM == 0;
}

struct TerraceFootprint {
  std::span<const PolygonRing> Rings;
  double BottomM = 0, TopM = 0;
};

struct TerraceRequest {
  LongitudeLatitude Anchor;
  unsigned SpacingM = 0;
  size_t SamplesMost = 0;
  size_t PointTestsMost = 0;
  std::stop_token Stop;
};

[[nodiscard]] std::expected<UrbanPrisms, std::string>
BuildFootprintTerraces(std::span<const TerraceFootprint> footprints, const TerraceRequest &request);
}

#endif
