#include "Check.h"
#include "FootprintTerraces.h"
#include <array>
#include <cstddef>
#include <stop_token>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = Generators::Osm;

PolygonRing Rectangle(double left, double top, double right, double bottom) {
  return {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
}
}

int main() {
  const std::array courtyard{Rectangle(0, 0, 24, 24), Rectangle(8, 8, 16, 16)};
  const std::array overlap{Rectangle(0, 0, 8, 8)};
  const std::array half{Rectangle(24, 0, 28, 8)};
  const std::array thin{Rectangle(40, 0, 42, 2)};
  const std::array footprints{Osm::TerraceFootprint{.Rings = courtyard, .BottomM = -2, .TopM = 10},
                              Osm::TerraceFootprint{.Rings = overlap, .BottomM = -4, .TopM = 16},
                              Osm::TerraceFootprint{.Rings = half, .BottomM = 2, .TopM = 8},
                              Osm::TerraceFootprint{.Rings = thin, .BottomM = 0, .TopM = 32}};
  constexpr size_t columns = 6;
  constexpr size_t rows = 3;
  constexpr size_t subsamples = 16;
  Osm::TerraceRequest demand{.Anchor = {.LongitudeDeg = 13, .LatitudeDeg = 48},
                             .SpacingM = 8,
                             .SamplesMost = columns * rows * subsamples,
                             .PointTestsMost = columns * rows * subsamples,
                             .Stop = {}};
  auto result = Osm::BuildFootprintTerraces(footprints, demand);
  CHECK(result && result->Geometry.Patches.size() == 1 && result->Geometry.Samples &&
            result->Geometry.Samples->size() == columns * rows,
        "a building block publishes one bounded grid over its source domain");
  if (result && result->Geometry.Samples && result->Geometry.Samples->size() == columns * rows) {
    const auto &cells = *result->Geometry.Samples;
    CHECK(cells[0].Lower == -4 && cells[0].Upper == 16,
          "overlapping footprints retain the highest roof and lowest base");
    CHECK(cells[columns + 1].Upper == 0 && cells[columns + 1].Lower == 0, "a courtyard stays open");
    CHECK(cells[3].Lower == 2 && cells[3].Upper == 8,
          "coverage at the half-cell threshold remains visible");
    CHECK(cells[5].Upper == 0 && cells[5].Lower == 0,
          "a point-sized source remains outside this coarse representation");
  }
  demand.SupportMostM = 16;
  CHECK(Osm::BuildFootprintTerraces(std::span(footprints).first(3), demand),
        "the occupied street block qualifies within the supported footprint size");
  CHECK(!Osm::BuildFootprintTerraces(footprints, demand),
        "losing the taller thin building would remove a visible part of the skyline");
  CHECK(!Osm::PartitionFootprintTerraces(footprints, demand),
        "refining the raster respects the total sampling budget across passes");
  demand.PointTestsMost *= 2;
  const auto mixed = Osm::PartitionFootprintTerraces(footprints, demand);
  CHECK(mixed && mixed->Retained.size() == 1 && mixed->Retained.front() == 3 &&
            mixed->Grid.Geometry.Patches.size() == 1,
        "the tall building stays in source geometry while the street block becomes a grid");
  demand.PointTestsMost = demand.SamplesMost;
  demand.SupportMostM = 0;
  --demand.SamplesMost;
  CHECK(!Osm::BuildFootprintTerraces(footprints, demand),
        "an over-budget raster cannot become a replacement asset");
  ++demand.SamplesMost;
  demand.PointTestsMost = 0;
  CHECK(!Osm::BuildFootprintTerraces(footprints, demand),
        "source sampling respects a separate work admission");
  demand.PointTestsMost = demand.SamplesMost;
  std::stop_source stop;
  demand.Stop = stop.get_token();
  (void)stop.request_stop();
  CHECK(!Osm::BuildFootprintTerraces(footprints, demand),
        "a stopped request publishes no replacement");
  return Report();
}
