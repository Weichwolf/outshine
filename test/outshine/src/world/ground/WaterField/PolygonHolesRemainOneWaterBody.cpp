#include "BuildingField.h"
#include "GroundSnapshot.h"
#include "OsmField.h"
#include "StreetField.h"
#include "TangentFrame.h"
#include "WaterField.h"
#include "WaterSurfaceBuilder.h"

#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;

void AppendVarint(Bytes &out, size_t value) {
  while (value >= 128) {
    out.push_back(static_cast<uint8_t>((value & 127u) | 128u));
    value >>= 7u;
  }
  out.push_back(static_cast<uint8_t>(value));
}

void Append(Bytes &out, uint8_t tag, std::span<const uint8_t> bytes) {
  out.push_back(tag);
  AppendVarint(out, bytes.size());
  out.insert(out.end(), bytes.begin(), bytes.end());
}

class Heights final : public outshine::GroundQuery {
public:
  bool Pending = true;
  mutable size_t Queries = 0;

  outshine::GroundSample At(outshine::LongitudeLatitude) const override {
    ++Queries;
    return Pending ? outshine::GroundSample::Waiting() : outshine::GroundSample::At(10.0);
  }

  outshine::GroundSample Resident(outshine::LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock BlockAt(outshine::Ground::TileSpot) const override {
    return outshine::Ground::GroundBlock::Waiting();
  }

  double PostM(double) const override { return 1.0; }
};

Bytes PolygonTile(bool oversizedHole) {
  std::vector<uint32_t> words{
      9, 0,  0,  26, 20, 0, 0, 20, 19, 0, 15, 9, 4, 15, oversizedHole ? 4098u : 26u,
      0, 12, 12, 0,  0,  11};
  if (oversizedHole) {
    for (size_t step = 0; step < 509; ++step) {
      words.push_back(step % 2u == 0 ? 2u : 1u);
      words.push_back(0);
    }
  }
  words.push_back(15);
  Bytes geometry;
  for (uint32_t word : words) {
    while (word >= 128) {
      geometry.push_back(static_cast<uint8_t>((word & 127u) | 128u));
      word >>= 7u;
    }
    geometry.push_back(static_cast<uint8_t>(word));
  }
  Bytes feature{0x18, 3};
  Append(feature, 0x22, geometry);
  Bytes layer{0x0a, 14,  'w', 'a', 't', 'e',  'r', '_',  'p',  'o', 'l',
              'y',  'g', 'o', 'n', 's', 0x78, 2,   0x28, 0x80, 0x20};
  Append(layer, 0x12, feature);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}

outshine::LongitudeLatitude
Midpoint(std::span<const double> points, uint32_t left, uint32_t right) {
  return {.LongitudeDeg = 0.5 * (points[left * 2u + 1u] + points[right * 2u + 1u]),
          .LatitudeDeg = 0.5 * (points[left * 2u] + points[right * 2u])};
}

bool CoveredBy(std::span<const float> positions,
               std::span<const uint32_t> triangles,
               outshine::EastNorth at) {
  for (size_t first = 0; first < triangles.size(); first += 3u) {
    bool inside = true;
    for (size_t edge = 0; edge < 3u; ++edge) {
      const size_t a = triangles[first + edge] * 3u;
      const size_t b = triangles[first + (edge + 1u) % 3u] * 3u;
      const double aE = positions[a];
      const double aN = -positions[a + 2u];
      const double bE = positions[b];
      const double bN = -positions[b + 2u];
      const double turn = (bE - aE) * (at.NorthM - aN) - (bN - aN) * (at.EastM - aE);
      if (turn < -1e-3) { inside = false; }
    }
    if (inside) { return true; }
  }
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;

  const std::array<std::string, 1> layers{"water_polygons"};
  OsmField field(6, layers);
  const auto accepted = field.Accept(32, 32, PolygonTile(false));
  CHECK(accepted.has_value() && field.Rings().size() == 2 && field.Rings()[0].Exterior &&
            !field.Rings()[1].Exterior,
        "OSM input retains outer and inner rings");
  if (!accepted) { return Report(); }
  Heights heights;
  VegetationTemplates vegetation;
  WaterField water;
  CHECK(water.Ingest(heights, field, vegetation) == 0 && water.Surfaces().empty(),
        "pending outer height publishes no partial water body");
  heights.Pending = false;
  for (int advance = 0; advance < 8 && !water.Ingested(field); ++advance) {
    (void)water.Ingest(heights, field, vegetation);
  }
  CHECK(water.Ingested(field) && water.Surfaces().size() == 1 &&
            water.RingsOf(water.Surfaces()[0]).size() == 2 && water.InvalidBodyCount() == 0,
        "outer ring and hole publish atomically as one water body");
  if (water.Surfaces().size() == 1) {
    const auto rings = water.RingsOf(water.Surfaces()[0]);
    CHECK(rings[0].FirstPoint == field.Rings()[0].First &&
              rings[1].FirstPoint == field.Rings()[1].First && water.Surfaces()[0].LevelM == 10.0f,
          "body preserves OSM ring order and outer level");
    BuildingField footprints;
    StreetField streets;
    Generators::Tile region(6, 32, 32);
    const auto features = Generators::FeaturesOver(
        region,
        {.Vectors = &field, .Footprints = &footprints, .WaterBodies = &water, .Ways = &streets});
    CHECK(features && features->Count() == 1 && features->Rings(features->At(0)).size() == 2,
          "native ground snapshot retains both rings");
    if (features && features->Count() == 1) {
      const auto points = field.Points();
      const auto wet = region.Enu(Midpoint(points, rings[0].FirstPoint, rings[1].FirstPoint));
      const auto inner =
          region.Enu(Midpoint(points, rings[1].FirstPoint, rings[1].FirstPoint + 2u));
      CHECK(features->Contains(features->At(0), wet) && !features->Contains(features->At(0), inner),
            "water occupies the shore band but leaves the island dry");
    }
    Geometry geometry;
    const auto material = geometry.addSurface("water", Material{});
    CHECK(material.has_value(), "native water material exists");
    if (material) {
      const auto points = field.Points();
      const TangentFrame frame =
          TangentFrame::At({.LongitudeDeg = points[1], .LatitudeDeg = points[0]});
      const auto built =
          Generators::AppendWaterSurfaceGeometry(geometry, *material, water, points, frame);
      CHECK(built && built->Laid == 1 && built->RefusedTopology == 0 && built->Triangles == 8 &&
                geometry.parts() == 1,
            "native triangulation cuts the outer polygon around the hole");
      if (built && geometry.parts() == 1) {
        const auto positions = geometry.positionsOf(0);
        const auto triangles = geometry.trianglesOf(0);
        const auto wetGeo = Midpoint(points, rings[0].FirstPoint, rings[1].FirstPoint);
        const auto innerGeo = Midpoint(points, rings[1].FirstPoint, rings[1].FirstPoint + 2u);
        const auto wet = frame.Place({.LongitudeDeg = wetGeo.LongitudeDeg,
                                      .LatitudeDeg = wetGeo.LatitudeDeg,
                                      .HeightM = 10.0});
        const auto inner = frame.Place({.LongitudeDeg = innerGeo.LongitudeDeg,
                                        .LatitudeDeg = innerGeo.LatitudeDeg,
                                        .HeightM = 10.0});
        CHECK(CoveredBy(positions, triangles, {.EastM = wet.EastM, .NorthM = wet.NorthM}) &&
                  !CoveredBy(positions, triangles, {.EastM = inner.EastM, .NorthM = inner.NorthM}),
              "render triangles cover water but never the island centre");
      }
    }
  }

  OsmField invalid(6, layers);
  const auto parsed = invalid.Accept(32, 32, PolygonTile(true));
  CHECK(parsed.has_value() && invalid.Rings().size() == 2 && invalid.Rings()[1].Count > 512,
        "oversized inner ring reaches WaterField as valid OSM topology");
  if (parsed) {
    Heights ready;
    ready.Pending = false;
    WaterField rejected;
    for (int advance = 0; advance < 8 && !rejected.Ingested(invalid); ++advance) {
      (void)rejected.Ingest(ready, invalid, vegetation);
    }
    CHECK(rejected.Ingested(invalid) && rejected.Surfaces().empty() &&
              rejected.InvalidBodyCount() == 1,
          "unusable hole rejects its whole body instead of filling the island");
  }

  OsmField concave(6, layers);
  const std::array concaveFeatures{OsmField::Declared{
      .Layer = "water_polygons",
      .Key = "kind",
      .Value = "lake",
      .Area = true,
      .LatLon = {0, 0, 0, 0.001, 0.0003, 0.001, 0.0003, 0.0003, 0.001, 0.0003, 0.001, 0}}};
  concave.Declare(concaveFeatures, TileAt{.X = 32, .Y = 32});
  Heights flat;
  flat.Pending = false;
  WaterField concaveWater;
  for (int advance = 0; advance < 8 && !concaveWater.Ingested(concave); ++advance) {
    (void)concaveWater.Ingest(flat, concave, vegetation);
  }
  Geometry concaveGeometry;
  const auto concaveMaterial = concaveGeometry.addSurface("water", Material{});
  CHECK(concaveMaterial && concaveWater.Surfaces().size() == 1,
        "concave lake has one native material and one accepted body");
  if (concaveMaterial && concaveWater.Surfaces().size() == 1) {
    const TangentFrame frame = TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0});
    const auto built = Generators::AppendWaterSurfaceGeometry(
        concaveGeometry, *concaveMaterial, concaveWater, concave.Points(), frame);
    CHECK(built && built->Triangles == 4 && built->RefusedTopology == 0,
          "concave polygon triangulates without a fan across its recess");
    if (built && concaveGeometry.parts() == 1) {
      const auto positions = concaveGeometry.positionsOf(0);
      const auto triangles = concaveGeometry.trianglesOf(0);
      const auto wet =
          frame.Place({.LongitudeDeg = 0.0006, .LatitudeDeg = 0.0001, .HeightM = 10.0});
      const auto recess =
          frame.Place({.LongitudeDeg = 0.0006, .LatitudeDeg = 0.0006, .HeightM = 10.0});
      CHECK(CoveredBy(positions, triangles, {.EastM = wet.EastM, .NorthM = wet.NorthM}) &&
                !CoveredBy(positions, triangles, {.EastM = recess.EastM, .NorthM = recess.NorthM}),
            "mesh covers the wet bar but leaves the concave recess dry");
    }
  }
  return Report();
}
