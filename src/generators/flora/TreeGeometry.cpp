#include "TreePrototype.h"
#include "TreeLeaf.h"
#include "TreeFrame.h"
#include <array>
#include <cmath>
#include <limits>

namespace outshine::Generators {
namespace {
struct Surface {
  std::vector<float> Positions, Normals, Uvs;
  std::vector<uint32_t> Indices;

  void Vertex(Vec3f p, Vec3f n, float u, float v) {
    Positions.insert(Positions.end(), {p[0], p[1], p[2]});
    Normals.insert(Normals.end(), {n[0], n[1], n[2]});
    Uvs.insert(Uvs.end(), {u, v});
  }

  bool Into(Geometry &geometry, const char *name, const Material &material) const {
    if (Indices.empty()) { return true; }
    const int part = geometry.addPart(name, geometry.addSurface(name, material));
    return part >= 0 && geometry.setPositions(part, Positions) &&
           geometry.setNormals(part, Normals) && geometry.setTexture(part, Uvs) &&
           geometry.setTriangles(part, Indices);
  }
};
} // namespace

std::optional<Geometry> TreePrototype::GeometryAt(size_t rank) const {
  if (rank >= Ranks_.size()) { return std::nullopt; }
  const Rank &source = Ranks_[rank];
  const auto height = static_cast<float>(HeightM_);
  Surface bark;
  for (size_t at = 0; at < source.BarkVerts.size(); at += TreeMesh::kBarkFloats) {
    const float *v = source.BarkVerts.data() + at;
    bark.Vertex(Vec3f{{v[0], v[1], v[2]}} * height,
                Vec3f{{v[3], v[4], v[5]}},
                v[0] * height,
                v[1] * height);
  }
  bark.Indices = source.BarkIdx;
  Material barkMaterial;
  barkMaterial.Roughness = Look_.BarkRoughness;
  for (int c = 0; c < 3; ++c) { barkMaterial.BaseColour[c] = Look_.BarkRgb[c]; }

  TreeMesh blade;
  TreeLeaf::Build(Leaf_, blade);
  Surface leaves;
  const size_t perCard = blade.LeafVertexCount();
  if (perCard > 0 && source.CardCount > std::numeric_limits<uint32_t>::max() / perCard) {
    return std::nullopt;
  }
  for (size_t at = 0; at < source.Cards.size(); at += 8) {
    const float *card = source.Cards.data() + at;
    const Vec3f origin = Vec3f{{card[0], card[1], card[2]}} * height;
    const Vec3f along = DirectionOrUp({{card[4], card[5], card[6]}});
    const Frame frame = FrameFrom({.Along = along, .Reference = {{0, 1, 0}}});
    const float roll = card[3];
    const Vec3f x = frame.Normal * std::cos(roll) + frame.Binormal * std::sin(roll);
    const Vec3f z = Cross(x, along);
    const auto first = static_cast<uint32_t>(leaves.Positions.size() / 3);
    for (size_t v = 0; v < blade.LeafVerts.size(); v += TreeMesh::kLeafFloats) {
      const float *p = blade.LeafVerts.data() + v;
      const Vec3f local = x * p[0] + along * p[1] + z * p[2];
      const Vec3f normal = DirectionOrUp(x * p[3] + along * p[4] + z * p[5]);
      leaves.Vertex(origin + local * source.CardLeafM, normal, p[6], p[7]);
    }
    for (const uint32_t index : blade.LeafIdx) { leaves.Indices.push_back(first + index); }
  }

  Material leafMaterial;
  leafMaterial.Roughness = Look_.LeafRoughness;
  leafMaterial.DoubleSided = true;
  for (int c = 0; c < 3; ++c) { leafMaterial.BaseColour[c] = Look_.LeafRgb[c]; }
  Geometry geometry;
  if (!bark.Into(geometry, "bark", barkMaterial) ||
      !leaves.Into(geometry, "leaves", leafMaterial)) {
    return std::nullopt;
  }
  return geometry;
}
} // namespace outshine::Generators
