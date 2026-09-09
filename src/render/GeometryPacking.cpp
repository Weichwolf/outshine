#include <expected>
#include "Shape.h"
#include "math/Mat4.h"
#include "scene/Geometry.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {

void AppendAttribute(std::span<const float> source,
                     const ShapePart &part,
                     size_t components,
                     std::span<const uint32_t> corners,
                     std::vector<float> &destination) {
  if (source.empty() && destination.empty()) { return; }
  destination.resize((part.FirstVertex + part.VertexCount) * components);
  if (source.empty()) { return; }
  if (corners.empty()) {
    std::ranges::copy(
        source, destination.begin() + static_cast<std::ptrdiff_t>(part.FirstVertex * components));
    return;
  }
  for (size_t corner = 0; corner < corners.size(); ++corner) {
    for (size_t component = 0; component < components; ++component) {
      destination[(part.FirstVertex + corner) * components + component] =
          source[static_cast<size_t>(corners[corner]) * components + component];
    }
  }
}

[[nodiscard]] std::span<const float> Attribute(const std::vector<float> &source,
                                               const ShapePart &part,
                                               size_t components,
                                               bool present) {
  if (!present) { return {}; }
  return std::span<const float>(source).subspan(part.FirstVertex * components,
                                                part.VertexCount * components);
}

[[nodiscard]] bool AttributesFit(const ShapePart &part, const ShapeStore &store) {
  const auto fits = [&part](size_t size, size_t components, bool present) {
    if (!present) { return true; }
    const size_t vertices = size / components;
    return size % components == 0 && part.FirstVertex <= vertices &&
           part.VertexCount <= vertices - part.FirstVertex;
  };
  return fits(store.PositionsM.size(), 3, true) && fits(store.Normals.size(), 3, part.HasNormal) &&
         fits(store.Tangents.size(), 4, part.HasTangent) && fits(store.Uv.size(), 2, part.HasUv) &&
         fits(store.Uv1.size(), 2, part.HasUv1) && fits(store.Colours.size(), 4, part.HasColour);
}

void GenerateFlatNormals(ShapePart &part, ShapeStore &into) {
  into.Normals.resize(into.PositionsM.size());
  const auto position = [&](size_t vertex) {
    const size_t at = vertex * 3;
    return Vec3{{into.PositionsM[at], into.PositionsM[at + 1], into.PositionsM[at + 2]}};
  };
  for (size_t vertex = part.FirstVertex; vertex < part.FirstVertex + part.VertexCount;
       vertex += 3) {
    const Vec3 origin = position(vertex);
    Vec3 normal = Cross(position(vertex + 1) - origin, position(vertex + 2) - origin);
    (void)Normalise(normal);
    for (size_t corner = 0; corner < 3; ++corner) {
      for (size_t axis = 0; axis < 3; ++axis) {
        into.Normals[(vertex + corner) * 3 + axis] = static_cast<float>(normal[axis]);
      }
    }
  }
  part.HasNormal = true;
}

[[nodiscard]] bool TransformPart(const Mat4 &placement, const ShapePart &part, ShapeStore &into) {
  if (placement == Mat4{}) { return false; }
  const Vec3 x = {{placement[0], placement[1], placement[2]}};
  const Vec3 y = {{placement[4], placement[5], placement[6]}};
  const Vec3 z = {{placement[8], placement[9], placement[10]}};
  const Vec3 nx = Cross(y, z);
  const Vec3 ny = Cross(z, x);
  const Vec3 nz = Cross(x, y);
  const double sign = Dot(x, nx) < 0 ? -1.0 : 1.0;
  for (size_t vertex = part.FirstVertex; vertex < part.FirstVertex + part.VertexCount; ++vertex) {
    const size_t at = vertex * 3;
    const Vec3 position = placement.TransformPoint(
        {{into.PositionsM[at], into.PositionsM[at + 1], into.PositionsM[at + 2]}});
    for (size_t axis = 0; axis < 3; ++axis) {
      into.PositionsM[at + axis] = static_cast<float>(position[axis]);
    }
    if (part.HasNormal) {
      Vec3 normal = (nx * static_cast<double>(into.Normals[at]) +
                     ny * static_cast<double>(into.Normals[at + 1]) +
                     nz * static_cast<double>(into.Normals[at + 2])) *
                    sign;
      (void)Normalise(normal);
      for (size_t axis = 0; axis < 3; ++axis) {
        into.Normals[at + axis] = static_cast<float>(normal[axis]);
      }
    }
    if (part.HasTangent) {
      const size_t along = vertex * 4;
      Vec3 tangent = placement.TransformDirection(
          {{into.Tangents[along], into.Tangents[along + 1], into.Tangents[along + 2]}});
      (void)Normalise(tangent);
      for (size_t axis = 0; axis < 3; ++axis) {
        into.Tangents[along + axis] = static_cast<float>(tangent[axis]);
      }
      into.Tangents[along + 3] *= static_cast<float>(sign);
    }
  }
  return sign < 0;
}

[[nodiscard]] ShapePart AppendGeometryPart(const Geometry &from, int part, ShapeStore &into) {
  ShapePart packed;
  packed.Name = from.nameOf(part);
  packed.Material = from.materialOf(part).index();
  packed.FirstVertex = into.PositionsM.size() / 3;
  const auto indices = from.trianglesOf(part);
  const bool flatNormals = from.normalsOf(part).empty() && !indices.empty();
  packed.VertexCount = flatNormals ? indices.size() : from.positionsOf(part).size() / 3;
  assert(packed.FirstVertex <= std::numeric_limits<uint32_t>::max());
  assert(packed.VertexCount <= std::numeric_limits<uint32_t>::max() - packed.FirstVertex);
  packed.FirstIndex = into.Indices.size();
  packed.IndexCount = from.trianglesOf(part).size();
  packed.HasNormal = !from.normalsOf(part).empty();
  packed.HasTangent = !from.tangentsOf(part).empty();
  packed.HasUv = !from.textureOf(part, Geometry::UvSet::Uv0).empty();
  packed.HasUv1 = !from.textureOf(part, Geometry::UvSet::Uv1).empty();
  packed.HasColour = !from.coloursOf(part).empty();
  const auto append =
      [&](std::span<const float> source, size_t components, std::vector<float> &destination) {
        AppendAttribute(source,
                        packed,
                        components,
                        flatNormals ? indices : std::span<const uint32_t>{},
                        destination);
      };
  append(from.positionsOf(part), 3, into.PositionsM);
  append(from.normalsOf(part), 3, into.Normals);
  append(from.tangentsOf(part), 4, into.Tangents);
  append(from.textureOf(part, Geometry::UvSet::Uv0), 2, into.Uv);
  append(from.textureOf(part, Geometry::UvSet::Uv1), 2, into.Uv1);
  append(from.coloursOf(part), 4, into.Colours);
  if (flatNormals) { GenerateFlatNormals(packed, into); }
  const bool mirrored = TransformPart(from.placementOf(part), packed, into);
  for (size_t corner = 0; corner < indices.size(); ++corner) {
    const auto vertex = flatNormals ? static_cast<uint32_t>(corner) : indices[corner];
    into.Indices.push_back(static_cast<uint32_t>(packed.FirstVertex) + vertex);
  }
  if (mirrored) {
    for (size_t at = packed.FirstIndex; at + 2 < into.Indices.size(); at += 3) {
      std::swap(into.Indices[at + 1], into.Indices[at + 2]);
    }
  }
  return packed;
}

}

void AppendGeometry(const Geometry &from, ShapeStore &into) {
  const auto firstSurface = static_cast<int>(into.Surfaces.size());
  for (int surface = 0; surface < from.surfaces(); ++surface) {
    into.Surfaces.push_back(from.surfaceAt(MaterialInstance(surface)));
  }
  for (int lamp = 0; lamp < from.lamps(); ++lamp) {
    PunctualLight light = from.lampAt(lamp);
    const Mat4 &placement = from.lampPlacementOf(lamp);
    const Vec3 position =
        placement.TransformPoint({{light.Position[0], light.Position[1], light.Position[2]}});
    Vec3 direction = placement.TransformDirection(
        {{light.Direction[0], light.Direction[1], light.Direction[2]}});
    (void)Normalise(direction);
    for (size_t axis = 0; axis < 3; ++axis) {
      light.Position[axis] = static_cast<float>(position[axis]);
      light.Direction[axis] = static_cast<float>(direction[axis]);
    }
    into.Lamps.push_back(light);
  }
  into.Parts.reserve(into.Parts.size() + static_cast<size_t>(from.parts()));
  for (int part = 0; part < from.parts(); ++part) {
    ShapePart packed = AppendGeometryPart(from, part, into);
    if (packed.Material >= 0) { packed.Material += firstSurface; }
    into.Parts.push_back(std::move(packed));
  }
}

std::expected<Shape, ClusterError> FinalizeShape(ShapeStore &into) {
  for (ShapePart &part : into.Parts) {
    if (!AttributesFit(part, into)) { return std::unexpected(ClusterError::InvalidLayout); }
    part.PositionsM = Attribute(into.PositionsM, part, 3, true);
    part.Normals = Attribute(into.Normals, part, 3, part.HasNormal);
    part.Tangents = Attribute(into.Tangents, part, 4, part.HasTangent);
    part.Uv = Attribute(into.Uv, part, 2, part.HasUv);
    part.Uv1 = Attribute(into.Uv1, part, 2, part.HasUv1);
    part.Colours = Attribute(into.Colours, part, 4, part.HasColour);
  }
  const auto cooked = CookShape(into, into.Surfaces);
  if (!cooked) { return std::unexpected(cooked.error()); }
  Shape view;
  view.Parts = into.Parts;
  view.Surfaces = into.Surfaces;
  view.Lamps = into.Lamps;
  view.Indices = into.Indices;
  view.Clusters = into.Clusters;
  view.ClusterSpheres = into.ClusterSpheres;
  for (const ShapePart &part : into.Parts) {
    view.CarriesUv = view.CarriesUv || part.HasUv;
    view.CarriesUv1 = view.CarriesUv1 || part.HasUv1;
    view.CarriesNormal = view.CarriesNormal || part.HasNormal;
    view.CarriesTangent = view.CarriesTangent || part.HasTangent;
    view.CarriesColour = view.CarriesColour || part.HasColour;
  }
  return view;
}

std::expected<Shape, ClusterError> PrepareShape(const Geometry &from, ShapeStore &into) {
  into.Clear();
  AppendGeometry(from, into);
  return FinalizeShape(into);
}

}
