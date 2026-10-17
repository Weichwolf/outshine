#include "Check.h"
#include "Shape.h"
#include <scene/Geometry.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kTriangles = 1024;
constexpr size_t kVertices = 3 * kTriangles;

Geometry Fixture() {
  Geometry geometry;
  const auto surface = geometry.addSurface("surface", {});
  CHECK(surface.has_value(), "fixture owns a material");
  if (!surface) { return geometry; }
  const auto large = geometry.addPart("large", *surface);
  const auto small = geometry.addPart("small", *surface);
  CHECK(large.has_value() && small.has_value(), "fixture owns two parts");
  if (!large || !small) { return geometry; }
  Geometry::Mesh mesh;
  for (uint32_t triangle = 0; triangle < kTriangles; ++triangle) {
    mesh.PositionsM.insert(mesh.PositionsM.end(), {0, 0, 0, 1, 0, 0, 0, 1, 0});
    mesh.Normals.insert(mesh.Normals.end(), {0, 0, 1, 0, 0, 1, 0, 0, 1});
    mesh.Colours.insert(mesh.Colours.end(), {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
    mesh.Indices.insert(mesh.Indices.end(), {3 * triangle, 3 * triangle + 1, 3 * triangle + 2});
  }
  CHECK(geometry.adoptMesh(*large, mesh).has_value(), "large part owns its arrays");
  Geometry::Mesh last{.PositionsM = {0, 0, 0, 1, 0, 0, 0, 1, 0},
                      .Normals = {},
                      .Uv = {0, 0, 1, 0, 0, 1},
                      .Uv1 = {1, 1, 0, 1, 1, 0},
                      .Tangents = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1},
                      .Colours = {},
                      .Indices = {0, 1, 2}};
  CHECK(geometry.adoptMesh(*small, last).has_value(),
        "small part introduces sparse attributes and needs flat normals");
  Mat4 mirror;
  mirror[0] = -1;
  mirror[12] = 10;
  CHECK(geometry.setPlacement(*small, mirror).has_value(), "small part has a reflected placement");
  return geometry;
}

template <typename T> void CheckCapacity(const std::vector<T> &values) {
  CHECK(values.capacity() <= values.size() + values.size() / 16 + 16,
        "a complete fresh product avoids almost doubling an attribute for its small last part");
}
}

int main() {
  const Geometry source = Fixture();
  Render::ShapeStore packed;
  Render::AppendGeometry(source, packed);
  CHECK(Render::BindShapeStorage(packed).has_value(), "packed attribute views are valid");
  if (packed.Parts.size() != 2 || packed.Indices.size() != kVertices + 3) { return Report() + 1; }
  CheckCapacity(packed.PositionsM);
  CheckCapacity(packed.Normals);
  CheckCapacity(packed.Tangents);
  CheckCapacity(packed.Uv);
  CheckCapacity(packed.Uv1);
  CheckCapacity(packed.Colours);
  CheckCapacity(packed.Indices);
  CHECK(packed.PositionsM[3 * kVertices] == 10 && packed.PositionsM[3 * kVertices + 3] == 9,
        "reflection and translation preserve the small part's positions");
  CHECK(packed.Indices[kVertices] == kVertices && packed.Indices[kVertices + 1] == kVertices + 2 &&
            packed.Indices[kVertices + 2] == kVertices + 1,
        "reflection reverses only the small part's winding");
  CHECK(packed.Normals[3 * kVertices + 2] == 1 && packed.Tangents[4 * kVertices] == -1 &&
            packed.Tangents[4 * kVertices + 3] == -1,
        "generated flat normals and reflected tangent handedness are preserved");
  CHECK(!packed.Parts[0].HasUv && packed.Parts[1].HasUv && packed.Parts[1].HasUv1 &&
            packed.Uv[2 * kVertices + 2] == 1 && packed.Colours[4 * kVertices] == 0,
        "late and absent attributes retain their original flags, offsets and zero fill");
  const auto positions = packed.PositionsM;
  Render::AppendGeometry(source, packed);
  CHECK(Render::BindShapeStorage(packed).has_value() && packed.Parts.size() == 4 &&
            packed.Indices.size() == 2 * (kVertices + 3),
        "appending to an existing product retains its complete earlier and new geometry");
  CHECK(std::equal(positions.begin(), positions.end(), packed.PositionsM.begin()),
        "appending preserves every previous position");
  const auto shaped = Render::FinalizeShape(packed);
  CHECK(shaped.has_value() && shaped->TriangleCount() == 2 * (kTriangles + 1),
        "cluster preparation accepts the complete combined product");
  packed.Clear();
  Render::AppendGeometry(source, packed);
  CHECK(Render::BindShapeStorage(packed).has_value() && packed.PositionsM == positions,
        "cleared reusable storage packs the same product exactly");
  return Report();
}
