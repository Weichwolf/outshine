#include <span>
#include <string_view>
#include "Meshed.h"
#include "Geodesy.h"
#include "math/RenderFrame.h"
#include "math/Vec3.h"

#include <string>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Generators {

namespace Says {
constexpr auto kInvalidTriangleSoup =
    "mesh requires nonempty complete triangles with uint32 indices";
}

bool Meshed::Take(std::string_view named,
                  MaterialInstance material,
                  std::span<const StoredVertex> soup,
                  const EnuAxes &frame) {
  const size_t vertices = soup.size();
  if (vertices == 0 || vertices % 3 != 0 || vertices > std::numeric_limits<uint32_t>::max()) {
    Error_ = Says::kInvalidTriangleSoup;
    return false;
  }

  std::vector<float> positionsM(vertices * 3);
  std::vector<float> uv(vertices * 2);
  std::vector<float> normalM(vertices * 3);
  std::vector<uint32_t> run(vertices);
  const auto local = [&frame](const Vec3f &value) {
    const Vec3 ecef{{value[0], value[1], value[2]}};
    return RenderFrame::Of({.EastM = Dot(ecef, frame.East),
                            .NorthM = Dot(ecef, frame.North),
                            .UpM = Dot(ecef, frame.Up)});
  };
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const StoredVertex &at = soup[vertex];
    const Vec2f texture = at.uv();
    const auto normal = local(at.norm());
    const auto position = local(at.pos);
    for (size_t axis = 0; axis < 3; ++axis) {
      positionsM[vertex * 3 + axis] = static_cast<float>(position[axis]);
      normalM[vertex * 3 + axis] = static_cast<float>(normal[axis]);
    }
    uv[vertex * 2] = texture[0];
    uv[vertex * 2 + 1] = texture[1];
    run[vertex] = static_cast<uint32_t>(vertex);
  }

  const int part = Held_.addPart(named, material);
  return Held_.setPositions(part, positionsM) && Held_.setTexture(part, uv) &&
         Held_.setNormals(part, normalM) && Held_.setTriangles(part, run);
}

}
