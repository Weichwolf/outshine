#include "PlantSurface.h"
#include "math/Vec3.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine::Generators {
namespace {
Vec3 Row(const std::vector<float> &values, uint32_t index) {
  const size_t at = static_cast<size_t>(index) * 3;
  return {{values[at], values[at + 1], values[at + 2]}};
}
}

bool PlantSurfaceIsOriented(const PlantSurface &mesh) noexcept {
  if (mesh.Positions.size() % 3 != 0 || mesh.Normals.size() != mesh.Positions.size() ||
      mesh.Triangles.size() % 3 != 0) {
    return false;
  }
  for (const auto index : mesh.Triangles) {
    if (index >= mesh.Positions.size() / 3) { return false; }
  }
  for (size_t at = 0; at < mesh.Triangles.size(); at += 3) {
    const Vec3 first = Row(mesh.Positions, mesh.Triangles[at]);
    const Vec3 cross = Cross(Row(mesh.Positions, mesh.Triangles[at + 1]) - first,
                             Row(mesh.Positions, mesh.Triangles[at + 2]) - first);
    for (size_t corner = 0; corner < 3; ++corner) {
      if (!(Dot(cross, Row(mesh.Normals, mesh.Triangles[at + corner])) > 0)) { return false; }
    }
  }
  return true;
}
}
