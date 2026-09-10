#include <span>
#include <string_view>
#include "Meshed.h"

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
                  std::span<const StoredVertex> soup) {
  const size_t vertices = soup.size();
  if (vertices == 0 || vertices % 3 != 0 || vertices > std::numeric_limits<uint32_t>::max()) {
    Error_ = Says::kInvalidTriangleSoup;
    return false;
  }

  std::vector<float> positionsM(vertices * 3);
  std::vector<float> uv(vertices * 2);
  std::vector<float> normalM(vertices * 3);
  std::vector<uint32_t> run(vertices);
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const StoredVertex &at = soup[vertex];
    const Vec2f texture = at.uv();
    const Vec3f normal = at.norm();
    for (size_t axis = 0; axis < 3; ++axis) {
      positionsM[vertex * 3 + axis] = at.pos[axis];
      normalM[vertex * 3 + axis] = normal[axis];
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
