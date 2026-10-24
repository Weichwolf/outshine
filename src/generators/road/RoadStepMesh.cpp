#include "RoadStepBody.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace outshine::Generators {
bool AppendRoadStepBody(const SurfaceBody &body, const Vec3f &colour, RoadMeshBuffers &mesh) {
  const size_t first = mesh.PositionM.size() / 3;
  if (first > std::numeric_limits<uint32_t>::max() ||
      body.Geometry.Faces.size() > (std::numeric_limits<uint32_t>::max() - first) / 3) {
    return false;
  }
  for (const auto face : body.Geometry.Faces) {
    std::array<Vec3f, 3> points;
    for (size_t at = 0; at < face.size(); ++at) {
      const auto source = body.Geometry.Points[face[at]];
      points[at] = {{static_cast<float>(source[0]),
                     static_cast<float>(source[2]),
                     static_cast<float>(-source[1])}};
    }
    const auto a = points[1] - points[0];
    const auto b = points[2] - points[0];
    const Vec3f cross = {
        {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
    const double length = std::hypot(cross[0], cross[1], cross[2]);
    const auto base = static_cast<uint32_t>(mesh.PositionM.size() / 3);
    for (const auto point : points) {
      for (size_t axis = 0; axis < 3; ++axis) {
        mesh.PositionM.push_back(point[axis]);
        mesh.NormalM.push_back(static_cast<float>(cross[axis] / length));
        mesh.ColourRgba.push_back(colour[axis]);
      }
      mesh.ColourRgba.push_back(1);
    }
    mesh.Index.insert(mesh.Index.end(), {base, base + 1, base + 2});
  }
  return true;
}
}
