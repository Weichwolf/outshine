#include "Check.h"
#include "OsmField.h"
#include "Shape.h"
#include "SubjectMaterials.h"
#include "TangentFrame.h"
#include "TerrainLoader.h"
#include "WaterField.h"
#include "WaterSurfaceBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace {
class BelowSeaLevel final : public outshine::GroundQuery {
public:
  outshine::GroundSample At(outshine::LongitudeLatitude) const override {
    return outshine::GroundSample::At(-10.0);
  }

  outshine::GroundSample Resident(outshine::LongitudeLatitude at) const override { return At(at); }

  outshine::Ground::GroundBlock BlockAt(outshine::Ground::TileSpot) const override {
    return outshine::Ground::GroundBlock::Waiting();
  }

  double PostM(double) const override { return 1.0; }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"water_polygons"};
  Ground::OsmField field(14, layers);
  const std::array features{
      Ground::OsmField::Declared{.Layer = "water_polygons",
                                 .Key = "kind",
                                 .Value = "lake",
                                 .Area = true,
                                 .LatLon = {0, 0, 0, 0.0001, 0.0001, 0.0001, 0.0001, 0}}};
  field.Declare(features, Ground::TileAt{.X = 8192, .Y = 8192});
  BelowSeaLevel heights;
  Ground::WaterField water;
  Ground::VegetationTemplates vegetation;
  for (int step = 0; step < 8 && !water.Ingested(field); ++step) {
    (void)water.Ingest(heights, field, vegetation);
  }
  CHECK(water.Surfaces().size() == 1 && water.Surfaces()[0].LevelM == -10.0f,
        "a known lake below sea level retains its own negative datum height");

  Geometry geometry;
  Material soil;
  soil.BaseColour = {{0.2f, 0.1f, 0.05f, 1.0f}};
  const auto ground = geometry.addSurface("ground", soil);
  const auto wet = geometry.addSurface("water", Generators::WaterSurfaceMaterial());
  CHECK(ground && wet && *ground != *wet, "terrain and water have different owning handles");
  if (!ground || !wet) { return Report(); }
  const auto terrain = geometry.addPart("terrain", *ground);
  CHECK(terrain.has_value(), "terrain part exists independently");
  if (!terrain) { return Report(); }
  CHECK(geometry.setPositions(*terrain, std::array<float, 9>{0, -12, 0, 10, -12, 0, 0, -12, -10}) &&
            geometry.setTriangles(*terrain, std::array<uint32_t, 3>{0, 1, 2}),
        "submerged terrain retains its geometry");
  const auto frame = TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0});
  const auto built =
      Generators::AppendWaterSurfaceGeometry(geometry, *wet, water, field.Points(), frame);
  CHECK(built && built->Laid == 1 && built->Triangles == 2 && geometry.parts() == 2,
        "generator appends a separate surface above its bed");
  if (!built || geometry.parts() != 2) { return Report(); }
  const auto positions = geometry.positionsOf(1);
  for (size_t at = 1; at < positions.size(); at += 3) {
    CHECK(std::abs(positions[at] + 10.0f) < 0.01f,
          "water geometry retains the source level instead of clamping to sea level");
  }
  Render::ShapeStore storage;
  const auto shape = Render::PrepareShape(geometry, storage);
  CHECK(shape.has_value(), "mixed terrain and water geometry prepares");
  if (!shape) { return Report(); }
  Core::SubjectMaterials materials;
  const auto resolved =
      materials.Resolve(geometry, *shape, soil, {}, ground->index(), "water-world");
  CHECK(resolved && materials.PartSlots().size() == 2, "native mixed materials resolve");
  if (!resolved || materials.PartSlots().size() != 2) { return Report(); }
  const auto &terrainMaterial = materials.Slots()[materials.PartSlots()[0]];
  const auto &waterMaterial = materials.Slots()[materials.PartSlots()[1]];
  CHECK(terrainMaterial.Domain == Render::SurfaceDomain::Ground &&
            waterMaterial.Domain == Render::SurfaceDomain::Subject,
        "only terrain receives the ground-classification shader");
  CHECK(terrainMaterial.State().Kind() == SurfaceKind::Opaque &&
            waterMaterial.State().Kind() == SurfaceKind::ThinTransmissive &&
            waterMaterial.Row.Metalness == 0 && waterMaterial.Row.Alpha == AlphaMode::Opaque,
        "water selects dielectric transmission with full coverage, separately from opaque ground");
  Vec3f f0;
  DielectricF0(waterMaterial.Row, f0);
  CHECK(std::ranges::all_of(f0, [](float value) { return value > 0.020f && value < 0.021f; }),
        "water reflects about two percent at normal incidence, derived from its refractive index");
  return Report();
}
