#include <array>
#include <cmath>
#include <scene/Geometry.h>
#include "Subject.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const int part = geometry.addPart("inclined plane", geometry.addSurface("dielectric", {}));
  const float unit = std::sqrt(0.5f);
  CHECK(geometry.setPositions(part, std::array<float, 9>{0, 0, 0, 1, 0, -1, 0, 1, 0}), "positions");
  CHECK(geometry.setNormals(part, std::array<float, 9>{unit, 0, unit, unit, 0, unit, unit, 0, unit}), "normals");
  CHECK(geometry.setTangents(part, std::array<float, 12>{unit, 0, -unit, 1, unit, 0, -unit, 1, unit, 0, -unit, 1}), "tangents");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}), "triangles");
  for (double mirror : {1.0, -1.0}) {
    Mat4 placement;
    placement[0] = 2 * mirror;
    placement[5] = 3;
    placement[10] = 4;
    placement.SetTranslation({{5, 7, 11}});
    CHECK(geometry.transforms().setTransform(part, placement), "native placement accepts affine scale and translation");
    Gltf::Subject subject;
    CHECK(subject.Assemble(geometry), "native geometry reaches the internal model");
    if (subject.VertexCount() != 3) { return Report(); }
    const std::array<double, 9> expected = {5, 7, 11, 5 + 2 * mirror, 7, 7, 5, 10, 11};
    for (size_t at = 0; at < expected.size(); ++at) {
      CHECK_NEAR(subject.PositionsM()[at], expected[at], 1e-12, "m", "placement applies to positions");
    }
    const double root = std::sqrt(5.0);
    CHECK_NEAR(subject.Normals()[0], 2 * mirror / root, 1e-7, "unit", "normal follows inverse transpose");
    CHECK_NEAR(subject.Normals()[2], 1 / root, 1e-7, "unit", "normal stays perpendicular after unequal scale");
    CHECK_NEAR(subject.Tangents()[0], mirror / root, 1e-7, "unit", "tangent follows the surface direction");
    CHECK_NEAR(subject.Tangents()[2], -2 / root, 1e-7, "unit", "tangent is normalized after scale");
    CHECK_NEAR(subject.Tangents()[3], mirror, 0, "sign", "reflection reverses tangent handedness");
    CHECK(subject.Indices()[1] == (mirror > 0 ? 1u : 2u), "reflection reverses triangle winding");
  }
  Mat4 lampPlacement;
  lampPlacement[0] = lampPlacement[10] = 0;
  lampPlacement[2] = -2;
  lampPlacement[5] = 3;
  lampPlacement[8] = 4;
  lampPlacement.SetTranslation({{5, 7, 11}});
  for (LightKind kind : {LightKind::Point, LightKind::Spot, LightKind::Directional}) {
    PunctualLight light;
    light.Kind = kind;
    light.Position = {{1, 2, 3}};
    light.Intensity = 37;
    light.RangeM = 19;
    CHECK(geometry.addLamp("placed lamp", light, lampPlacement) >= 0, "native light placement");
  }
  Gltf::Subject lit;
  CHECK(lit.Assemble(geometry), "native lights reach the internal model");
  Gltf::Subject roundtrip;
  CHECK(roundtrip.Assemble(lit.Handed()), "handing out and reassembling preserves light space");
  for (const Gltf::Subject *subject : {&lit, &roundtrip}) {
    CHECK(subject->Lights().size() == 3, "all three punctual light types survive");
    for (const auto &placed : subject->Lights()) {
      const auto &light = placed.Light;
      if (light.Kind != LightKind::Directional) {
        CHECK(light.Position == Vec3f({{17, 13, 9}}), "local light position is transformed once");
      }
      if (light.Kind != LightKind::Point) {
        CHECK(light.Direction == Vec3f({{-1, 0, 0}}), "light beam inherits orientation and remains unit length");
      }
      CHECK(light.Intensity == 37 && light.RangeM == 19, "scale changes neither photometric intensity nor range");
    }
  }
  return Report();
}
