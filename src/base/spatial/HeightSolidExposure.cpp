#include "HeightSolid.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace outshine {
bool HeightSolidExposesFace(const HeightSolid &body, size_t face) noexcept {
  if (face >= body.Faces.size()) { return false; }
  if (body.SurfaceFaces > body.Faces.size() / 2) { return true; }
  const size_t firstWall = 2 * body.SurfaceFaces;
  if (face < firstWall || face - firstWall >= body.WallKinds.size()) { return true; }
  return body.WallKinds[face - firstWall] != HeightSolidWallKind::Interface;
}

std::optional<std::array<uint32_t, 2>> HeightSolidWallSurfaceEdge(const HeightSolid &body,
                                                                  size_t wall) noexcept {
  if (wall >= body.WallParents.size() || body.SurfaceFaces > body.Faces.size() / 2 ||
      wall >= body.Faces.size() - 2 * body.SurfaceFaces ||
      body.WallParents[wall] >= body.SurfaceFaces) {
    return {};
  }
  const auto triangle = body.Faces[2 * body.SurfaceFaces + wall];
  const auto top = body.Faces[body.WallParents[wall]];
  std::array<uint32_t, 3> points{};
  for (size_t at = 0; at < 3; ++at) {
    if (triangle[at] >= body.Points.size()) { return {}; }
    const auto &point = body.Points[triangle[at]];
    bool found = false;
    for (const auto vertex : top) {
      if (vertex >= body.Points.size()) { return {}; }
      const auto &candidate = body.Points[vertex];
      if (candidate[0] == point[0] && candidate[1] == point[1]) {
        points[at] = vertex;
        found = true;
        break;
      }
    }
    if (!found) { return {}; }
  }
  const auto [first, last] = std::minmax({points[0], points[1], points[2]});
  if (first == last) { return {}; }
  return std::array{first, last};
}
}
