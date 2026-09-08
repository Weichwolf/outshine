#include <array>
#include <cmath>
#include <scene/Geometry.h>
#include "Subject.h"
#include "Shape.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const int part = geometry.addPart("inclined plane", geometry.addSurface("dielectric", {}));
  const float unit = std::sqrt(0.5f);
  CHECK(geometry.setPositions(part, std::array<float, 9>{0, 0, 0, 1, 0, -1, 0, 1, 0}), "positions");
  CHECK(
      geometry.setNormals(part, std::array<float, 9>{unit, 0, unit, unit, 0, unit, unit, 0, unit}),
      "normals");
  CHECK(geometry.setTangents(
            part, std::array<float, 12>{unit, 0, -unit, 1, unit, 0, -unit, 1, unit, 0, -unit, 1}),
        "tangents");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}), "triangles");
  for (double mirror : {1.0, -1.0}) {
    Mat4 placement;
    placement[0] = 2 * mirror;
    placement[5] = 3;
    placement[10] = 4;
    placement.SetTranslation({{5, 7, 11}});
    CHECK(geometry.transforms().setTransform(part, placement),
          "native placement accepts affine scale and translation");
    Render::ShapeStore nativeStorage;
    const Render::Shape native = Render::PrepareShape(geometry, nativeStorage);
    const Box nativeBounds = native.BoundsOf(0);
    CHECK_NEAR(nativeBounds.Min[0],
               mirror > 0 ? 5.0 : 3.0,
               1e-6,
               "m",
               "native bounds include reflected local-to-model X placement");
    CHECK_NEAR(nativeBounds.Max[0],
               mirror > 0 ? 7.0 : 5.0,
               1e-6,
               "m",
               "native bounds include local-to-model X scale");
    CHECK_NEAR(nativeBounds.Min[1], 7.0, 1e-6, "m", "native bounds include Y translation");
    CHECK_NEAR(nativeBounds.Max[1], 10.0, 1e-6, "m", "native bounds include Y scale");
    CHECK_NEAR(nativeBounds.Min[2], 7.0, 1e-6, "m", "native bounds include Z scale");
    CHECK_NEAR(nativeBounds.Max[2], 11.0, 1e-6, "m", "native bounds include Z translation");
    Gltf::Subject subject;
    CHECK(subject.Assemble(geometry), "native geometry reaches the internal model");
    if (subject.VertexCount() != 3) { return Report(); }
    const std::array<double, 9> expected = {5, 7, 11, 5 + 2 * mirror, 7, 7, 5, 10, 11};
    for (size_t at = 0; at < expected.size(); ++at) {
      CHECK_NEAR(
          subject.PositionsM()[at], expected[at], 1e-12, "m", "placement applies to positions");
    }
    const double root = std::sqrt(5.0);
    CHECK_NEAR(native.Parts[0].Normals[0],
               2 * mirror / root,
               1e-7,
               "unit",
               "native packed normal follows inverse transpose");
    CHECK_NEAR(native.Parts[0].Normals[2],
               1 / root,
               1e-7,
               "unit",
               "native packed normal remains perpendicular after unequal scale");
    CHECK_NEAR(native.Parts[0].Tangents[0],
               mirror / root,
               1e-7,
               "unit",
               "native packed tangent follows the scaled surface");
    CHECK_NEAR(native.Parts[0].Tangents[2],
               -2 / root,
               1e-7,
               "unit",
               "native packed tangent remains normalized");
    CHECK_NEAR(native.Parts[0].Tangents[3],
               mirror,
               0,
               "sign",
               "native reflection reverses tangent handedness");
    CHECK(native.Indices[1] == (mirror > 0 ? 1u : 2u),
          "native reflection preserves CCW front faces");
    CHECK_NEAR(
        subject.Normals()[0], 2 * mirror / root, 1e-7, "unit", "normal follows inverse transpose");
    CHECK_NEAR(subject.Normals()[2],
               1 / root,
               1e-7,
               "unit",
               "normal stays perpendicular after unequal scale");
    CHECK_NEAR(subject.Tangents()[0],
               mirror / root,
               1e-7,
               "unit",
               "tangent follows the surface direction");
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
  Render::ShapeStore nativeStorage;
  const Render::Shape native = Render::PrepareShape(geometry, nativeStorage);
  CHECK(native.Lamps.size() == 3, "native render input preserves every light type");
  for (const auto &light : native.Lamps) {
    if (light.Kind != LightKind::Directional) {
      CHECK(light.Position == Vec3f({{17, 13, 9}}),
            "native light position includes its local offset exactly once");
    }
    if (light.Kind != LightKind::Point) {
      CHECK(light.Direction == Vec3f({{-1, 0, 0}}),
            "native light direction includes orientation and normalization");
    }
    CHECK(light.Intensity == 37 && light.RangeM == 19,
          "native placement preserves photometric units and range");
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
        CHECK(light.Direction == Vec3f({{-1, 0, 0}}),
              "light beam inherits orientation and remains unit length");
      }
      CHECK(light.Intensity == 37 && light.RangeM == 19,
            "scale changes neither photometric intensity nor range");
    }
  }
  Render::AppendGeometry(geometry, nativeStorage);
  const Render::Shape combined = Render::FinalizeShape(nativeStorage);
  CHECK(combined.Parts.size() == 2 && combined.Lamps.size() == 6,
        "append retains existing render input and appends each native part and light");
  for (size_t packedPart = 0; packedPart < combined.Parts.size(); ++packedPart) {
    CHECK_NEAR(combined.Parts[packedPart].PositionsM[0],
               5.0,
               1e-6,
               "m",
               "all attribute views are rebound after append reallocates storage");
    CHECK(combined.Parts[packedPart].Material == static_cast<int>(packedPart),
          "append rebases material indices");
    CHECK(combined.Indices[packedPart * 3 + 1] == packedPart * 3 + 2,
          "append rebases mirrored local triangle indices");
  }
  geometry.clear();
  CHECK_NEAR(combined.BoundsOf(0).Min[0],
             3.0,
             1e-6,
             "m",
             "packed positions remain valid after clearing their source geometry");
  CHECK(combined.Parts[0].Name == "inclined plane" && combined.Lamps.size() == 6,
        "packed names and lights belong to the render storage");
  Geometry faceted;
  const int facePart = faceted.addPart("shared hard edge", MaterialInstance{});
  CHECK(faceted.setPositions(facePart, std::array<float, 12>{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1}) &&
            faceted.setTexture(facePart, std::array<float, 8>{0, 0, 1, 0, 0, 1, 1, 1}) &&
            faceted.setTriangles(facePart, std::array<uint32_t, 6>{0, 1, 2, 0, 3, 1}),
        "two noncoplanar triangles share vertices without authored normals");
  Render::ShapeStore flatStorage;
  const auto flat = Render::PrepareShape(faceted, flatStorage);
  CHECK(flat.Parts.size() == 1 && flat.Parts[0].HasNormal && flat.Parts[0].VertexCount == 6,
        "flat shading splits shared corners in the derived mesh");
  if (flat.Parts.size() == 1 && flat.Parts[0].Normals.size() == 18) {
    const auto normals = flat.Parts[0].Normals;
    for (size_t corner = 0; corner < 6; ++corner) {
      CHECK_NEAR(normals[corner * 3], 0, 0, "unit", "both faces have zero X normal");
      CHECK_NEAR(normals[corner * 3 + 1], corner < 3 ? 0 : 1, 0, "unit", "second face points +Y");
      CHECK_NEAR(normals[corner * 3 + 2], corner < 3 ? 1 : 0, 0, "unit", "first face points +Z");
    }
    CHECK(flat.Parts[0].Uv.size() == 12 && flat.Parts[0].Uv[8] == 1 && flat.Parts[0].Uv[9] == 1,
          "corner expansion preserves the fourth source vertex's UV");
    CHECK(flat.Indices.size() == 6 && flat.Indices[0] != flat.Indices[3],
          "adjacent faces cannot interpolate their different flat normals");
  } else {
    CHECK(false, "every expanded corner has a normal");
  }
  CHECK(faceted.positionsOf(facePart).size() == 12 && faceted.normalsOf(facePart).empty(),
        "derived flat normals do not mutate native source geometry");
  return Report();
}
