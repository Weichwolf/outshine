#include "TreePrototype.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "TreeLeaf.h"
#include "TreeFoliage.h"
#include "TreeFrame.h"
#include "ModelLadder.h"
#include <array>
#include <cmath>
#include <limits>

namespace outshine::Generators {
namespace {
struct VertexInput {
  Vec3f Position;
  Vec3f Normal;
  std::array<float, 2> Uv;
};

struct Surface {
  std::vector<float> Positions, Normals, Uvs;
  std::vector<uint32_t> Indices;

  void AppendVertex(const VertexInput &vertex) {
    Positions.insert(Positions.end(), {vertex.Position[0], vertex.Position[1], vertex.Position[2]});
    Normals.insert(Normals.end(), {vertex.Normal[0], vertex.Normal[1], vertex.Normal[2]});
    Uvs.insert(Uvs.end(), vertex.Uv.begin(), vertex.Uv.end());
  }

  bool Into(Geometry &geometry, const char *name, const Material &material) const {
    if (Indices.empty()) { return true; }
    const auto surface = geometry.addSurface(name, material);
    if (!surface) { return false; }
    const auto createdPart = geometry.addPart(name, *surface);
    if (!createdPart) { return false; }
    const int part = *createdPart;
    return part >= 0 && geometry.setPositions(part, Positions) &&
           geometry.setNormals(part, Normals) && geometry.setTexture(part, Uvs) &&
           geometry.setTriangles(part, Indices);
  }
};

struct LeafFrame {
  Vec3f Origin, X, Y, Z;
  float Scale;

  Vec3f Position(const float *p) const { return Origin + (X * p[0] + Y * p[1] + Z * p[2]) * Scale; }

  Vec3f Normal(const float *p) const { return DirectionOrUp(X * p[3] + Y * p[4] + Z * p[5]); }

  [[nodiscard]] Mat4 ModelMatrix() const {
    Mat4 result;
    const std::array<Vec3f, 3> axes{X, Y, Z};
    for (size_t column = 0; column < 3; ++column) {
      for (size_t row = 0; row < 3; ++row) { result.At(row, column) = axes[column][row] * Scale; }
    }
    for (size_t row = 0; row < 3; ++row) { result.At(row, 3) = Origin[row]; }
    return result;
  }
};

LeafFrame FrameOf(const float *card, float height, float scale) {
  const Vec3f along = DirectionOrUp({{card[4], card[5], card[6]}});
  const Frame frame = FrameFrom({.Along = along, .Reference = {{0, 1, 0}}});
  const Vec3f x = frame.Normal * std::cos(card[3]) + frame.Binormal * std::sin(card[3]);
  return {.Origin = Vec3f{{card[0], card[1], card[2]}} * height,
          .X = x,
          .Y = along,
          .Z = Cross(x, along),
          .Scale = scale};
}

Surface BarkOf(const TreePrototype::Rank &source, float height) {
  Surface bark;
  for (size_t at = 0; at < source.BarkVerts.size(); at += TreeMesh::kBarkFloats) {
    const float *v = source.BarkVerts.data() + at;
    bark.AppendVertex({.Position = Vec3f{{v[0], v[1], v[2]}} * height,
                       .Normal = {{v[3], v[4], v[5]}},
                       .Uv = {v[0] * height, v[1] * height}});
  }
  bark.Indices = source.BarkIdx;
  return bark;
}

Material MaterialOf(const Vec3f &colour, float roughness, bool doubleSided) {
  Material result;
  result.Roughness = roughness;
  result.DoubleSided = doubleSided;
  for (size_t c = 0; c < 3; ++c) { result.BaseColour[c] = colour[c]; }
  return result;
}

std::optional<TreeMesh> BladeOf(const TreeSpecies::Leaf &leaf,
                                const TreePrototype::Rank &source,
                                float height,
                                size_t rank) {
  const auto relativeDeviation = ModelLadder::RelativeDeviation(rank);
  if (!relativeDeviation) { return std::nullopt; }
  TreeMesh blade;
  const float deviation =
      rank > 0 && source.CardLeafM > 0 ? *relativeDeviation * height / (2 * source.CardLeafM) : 0;
  if (!TreeLeaf::Build(leaf, blade, {.MaxDeviation = deviation})) { return std::nullopt; }
  return blade;
}
}

std::optional<Geometry> TreePrototype::GeometryAt(size_t rank) const {
  if (rank >= Ranks_.size()) { return std::nullopt; }
  const Rank &source = Ranks_[rank];
  const auto height = static_cast<float>(HeightM_);
  const Surface bark = BarkOf(source, height);
  const Material barkMaterial = MaterialOf(Look_.BarkRgb, Look_.BarkRoughness, false);
  const auto blade = BladeOf(Leaf_, source, height, rank);
  if (!blade) { return std::nullopt; }
  Surface leaves;
  const size_t perCard = blade->LeafVertexCount();
  if (perCard > 0 && source.CardCount > std::numeric_limits<uint32_t>::max() / perCard) {
    return std::nullopt;
  }
  for (size_t at = 0; at < source.Cards.size(); at += TreeFoliage::kFloats) {
    const float *card = source.Cards.data() + at;
    const auto frame = FrameOf(card, height, source.CardLeafM);
    const auto first = static_cast<uint32_t>(leaves.Positions.size() / 3);
    for (size_t v = 0; v < blade->LeafVerts.size(); v += TreeMesh::kLeafFloats) {
      const float *p = blade->LeafVerts.data() + v;
      leaves.AppendVertex(
          {.Position = frame.Position(p), .Normal = frame.Normal(p), .Uv = {p[6], p[7]}});
    }
    for (const uint32_t index : blade->LeafIdx) { leaves.Indices.push_back(first + index); }
  }

  const Material leafMaterial = MaterialOf(Look_.LeafRgb, Look_.LeafRoughness, true);
  Geometry geometry;
  if (!bark.Into(geometry, "bark", barkMaterial) ||
      !leaves.Into(geometry, "leaves", leafMaterial)) {
    return std::nullopt;
  }
  return geometry;
}

std::optional<TreePrototype::InstancedGeometry>
TreePrototype::InstancedGeometryAt(size_t rank) const {
  if (rank >= Ranks_.size()) { return std::nullopt; }
  const auto &source = Ranks_[rank];
  const auto height = static_cast<float>(HeightM_);
  const auto bark = BarkOf(source, height);
  const auto blade = BladeOf(Leaf_, source, height, rank);
  if (!blade) { return std::nullopt; }
  Surface leaf;
  for (size_t at = 0; at < blade->LeafVerts.size(); at += TreeMesh::kLeafFloats) {
    const float *p = blade->LeafVerts.data() + at;
    leaf.AppendVertex(
        {.Position = {{p[0], p[1], p[2]}}, .Normal = {{p[3], p[4], p[5]}}, .Uv = {p[6], p[7]}});
  }
  leaf.Indices = blade->LeafIdx;
  InstancedGeometry result;
  if (!bark.Into(result.Bark, "bark", MaterialOf(Look_.BarkRgb, Look_.BarkRoughness, false)) ||
      !leaf.Into(result.Leaf, "leaves", MaterialOf(Look_.LeafRgb, Look_.LeafRoughness, true))) {
    return std::nullopt;
  }
  result.LeastM = {{std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity()}};
  result.MostM = result.LeastM * -1.0;
  const auto bound = [&](Vec3f p) {
    for (size_t axis = 0; axis < 3; ++axis) {
      result.LeastM[axis] = std::min(result.LeastM[axis], static_cast<double>(p[axis]));
      result.MostM[axis] = std::max(result.MostM[axis], static_cast<double>(p[axis]));
    }
  };
  for (size_t at = 0; at < bark.Positions.size(); at += 3) {
    bound({{bark.Positions[at], bark.Positions[at + 1], bark.Positions[at + 2]}});
  }
  result.Placements.reserve(source.CardCount);
  for (size_t at = 0; at < source.Cards.size(); at += TreeFoliage::kFloats) {
    const auto frame = FrameOf(source.Cards.data() + at, height, source.CardLeafM);
    result.Placements.push_back(frame.ModelMatrix());
    for (size_t v = 0; v < blade->LeafVerts.size(); v += TreeMesh::kLeafFloats) {
      bound(frame.Position(blade->LeafVerts.data() + v));
    }
  }
  return result;
}
}
