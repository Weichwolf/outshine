#include <algorithm>
#include <span>
#include <string_view>
#include "BuildingMeshConversion.h"
#include "Geodesy.h"
#include "math/RenderFrame.h"
#include "Vec3.h"

#include <expected>
#include <limits>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators {

std::expected<Geometry::Mesh, std::string_view>
ConvertBuildingMesh(const BuildingMeshPart &part, const EnuAxes &frame, const Vec3 &origin) {
  const size_t vertices = part.Vertices.size();
  if (vertices == 0 || vertices > std::numeric_limits<uint32_t>::max() || part.Indices.empty() ||
      part.Indices.size() % 3 != 0 ||
      (!part.Colours.empty() && part.Colours.size() != vertices * 4) ||
      (!part.MaterialM.empty() && part.MaterialM.size() != vertices * 2) ||
      std::ranges::any_of(part.Indices, [vertices](uint32_t index) { return index >= vertices; })) {
    return std::unexpected(
        "building mesh requires complete indexed vertices and matching material attributes");
  }
  Geometry::Mesh mesh;
  mesh.PositionsM.resize(vertices * 3);
  mesh.Uv.resize(vertices * 2);
  mesh.Normals.resize(vertices * 3);
  mesh.Indices.assign(part.Indices.begin(), part.Indices.end());
  mesh.Colours.assign(part.Colours.begin(), part.Colours.end());
  mesh.Uv1.assign(part.MaterialM.begin(), part.MaterialM.end());
  const auto local = [&frame](const Vec3 &ecef) {
    return RenderFrame::Of({.EastM = Dot(ecef, frame.East),
                            .NorthM = Dot(ecef, frame.North),
                            .UpM = Dot(ecef, frame.Up)});
  };
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const StoredVertex &at = part.Vertices[vertex];
    const Vec2f texture = at.uv();
    const auto n = at.norm();
    const auto normal = local(Vec3{{n[0], n[1], n[2]}});
    const auto position = local(Vec3{{at.pos[0], at.pos[1], at.pos[2]}} - origin);
    for (size_t axis = 0; axis < 3; ++axis) {
      mesh.PositionsM[vertex * 3 + axis] = static_cast<float>(position[axis]);
      mesh.Normals[vertex * 3 + axis] = static_cast<float>(normal[axis]);
    }
    mesh.Uv[vertex * 2] = texture[0];
    mesh.Uv[vertex * 2 + 1] = texture[1];
  }
  return mesh;
}

}
