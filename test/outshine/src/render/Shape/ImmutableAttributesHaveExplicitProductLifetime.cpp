#include "Check.h"
#include "Shape.h"
#include <scene/Geometry.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry Fixture() {
  Geometry geometry;
  const auto surface = geometry.addSurface("surface", {});
  CHECK(surface, "fixture owns its material");
  if (!surface) { return geometry; }
  for (int at = 0; at < 4; ++at) {
    const auto part = geometry.addPart("part", *surface);
    CHECK(part, "fixture owns its parts");
    if (!part) { return geometry; }
    const auto offset = static_cast<float>(at * 4);
    Geometry::Mesh mesh;
    mesh.PositionsM = {offset, 0, 0, offset + 1, 0, 0, offset, 1, 0, offset + 1, 1, 0};
    mesh.Indices = {0, 1, 2, 2, 1, 3};
    if (at != 2) { mesh.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}; }
    if (at == 0 || at == 2) { mesh.Uv = {0, 0, 1, 0, 0, 1, 1, 1}; }
    if (at == 1 || at == 3) { mesh.Uv1 = {1, 1, 0, 1, 1, 0, 0, 0}; }
    if (at == 1) { mesh.Tangents = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}; }
    if (at == 0 || at == 3) { mesh.Colours = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1}; }
    CHECK(geometry.adoptMesh(*part, mesh), "fixture adopts its complete arrays");
  }
  Mat4 mirrored;
  mirrored[0] = -2;
  mirrored[5] = 3;
  mirrored[10] = 4;
  mirrored[12] = 2;
  CHECK(geometry.setPlacement(1, mirrored), "one part needs reflected nonuniform conversion");
  PunctualLight lamp;
  lamp.Kind = LightKind::Point;
  Mat4 lightPlacement;
  lightPlacement[12] = 4;
  lightPlacement[13] = 2;
  CHECK(geometry.addLamp("lamp", lamp, lightPlacement), "fixture owns an independent lamp");
  return geometry;
}

void Equivalent(const Render::Shape &left, const Render::Shape &right) {
  CHECK(left.Parts.size() == right.Parts.size() && left.Lamps.size() == right.Lamps.size(),
        "part and lamp counts preserve the complete product");
  CHECK(std::ranges::equal(left.Indices, right.Indices), "owned cook indices match exactly");
  constexpr std::array attributes{&Render::ShapePart::PositionsM,
                                  &Render::ShapePart::Normals,
                                  &Render::ShapePart::Tangents,
                                  &Render::ShapePart::Uv,
                                  &Render::ShapePart::Uv1,
                                  &Render::ShapePart::Colours};
  for (size_t at = 0; at < std::min(left.Parts.size(), right.Parts.size()); ++at) {
    const auto &first = left.Parts[at];
    const auto &second = right.Parts[at];
    CHECK(first.Name == second.Name && first.Material == second.Material &&
              first.FirstVertex == second.FirstVertex && first.VertexCount == second.VertexCount &&
              first.FirstIndex == second.FirstIndex && first.IndexCount == second.IndexCount &&
              first.FirstCluster == second.FirstCluster &&
              first.ClusterCount == second.ClusterCount,
          "logical offsets and cooked cluster ranges do not depend on attribute ownership");
    CHECK(first.HasNormal == second.HasNormal && first.HasTangent == second.HasTangent &&
              first.HasUv == second.HasUv && first.HasUv1 == second.HasUv1 &&
              first.HasColour == second.HasColour,
          "sparse attribute flags are unchanged");
    for (const auto attribute : attributes) {
      CHECK(std::ranges::equal(first.*attribute, second.*attribute),
            "every attribute matches the complete owned conversion bit for bit");
    }
  }
  for (size_t at = 0; at < std::min(left.Lamps.size(), right.Lamps.size()); ++at) {
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK(left.Lamps[at].Position[axis] == right.Lamps[at].Position[axis] &&
                left.Lamps[at].Direction[axis] == right.Lamps[at].Direction[axis],
            "lamp transforms remain independent from geometry attribute ownership");
    }
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto original = std::make_shared<const Geometry>(Fixture());
  const std::weak_ptr<const Geometry> weak = original;
  Render::ShapeStore copied;
  Render::ShapeStore shared;
  const auto expected = Render::PrepareShape(*original, copied);
  const auto actual = Render::PrepareShape(original, shared);
  CHECK(expected && actual, "owned and retained products both cook successfully");
  if (!expected || !actual || shared.Parts.size() != 4) { return Report() + 1; }
  Equivalent(*expected, *actual);
  CHECK(shared.AttributeSource == original && shared.PositionsM.size() == size_t{3} * (4 + 6),
        "only transformed and flat-normal vertices need owned attribute storage");
  for (const int part : {0, 3}) {
    CHECK(shared.Parts[static_cast<size_t>(part)].PositionsM.data() ==
              original->positionsOf(part).data(),
          "unchanged parts directly view their immutable original arrays");
  }
  for (const int part : {1, 2}) {
    CHECK(shared.Parts[static_cast<size_t>(part)].PositionsM.data() !=
              original->positionsOf(part).data(),
          "transformation and generated attributes use independent working storage");
  }
  constexpr std::array<uint32_t, 6> corners{0, 1, 2, 2, 1, 3};
  CHECK(std::ranges::equal(original->trianglesOf(0), corners),
        "cooking never reorders original indices");
  original.reset();
  CHECK(!weak.expired() && shared.Parts[3].PositionsM[0] == 12,
        "views retain their real immutable product after the caller releases it");
  Render::AppendGeometry(*shared.AttributeSource, shared);
  const auto appended = Render::FinalizeShape(shared);
  CHECK(appended && shared.Parts.size() == 8 && !shared.AttributeSource && weak.expired(),
        "append promotes mixed views before releasing their owner, including source aliasing");
  if (appended) {
    for (size_t part = 0; part < 4; ++part) {
      CHECK(std::ranges::equal(shared.Parts[part].PositionsM, copied.Parts[part].PositionsM),
            "ownership promotion preserves already cooked part positions");
    }
  }
  return Report();
}
