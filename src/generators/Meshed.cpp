#include "Meshed.h"

#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Generators {

bool Meshed::Take(std::string named, MaterialInstance material, const float *soup, size_t floats) {
  if (soup == nullptr || floats == 0) {
    Error_ = "a part of no vertices is a refusal, not an empty mesh";
    return false;
  }
  if (floats % kSoupFloatsPerVertex != 0) {
    Error_ = "a soup of " + std::to_string(floats) + " float(s) is not a whole number of " +
             std::to_string(kSoupFloatsPerVertex) + "-float vertices";
    return false;
  }
  const size_t vertices = floats / kSoupFloatsPerVertex;
  if (vertices % 3 != 0) {
    Error_ =
        "a soup of " + std::to_string(vertices) + " vertices is not a whole number of triangles";
    return false;
  }

  std::vector<float> positionsM(vertices * 3);
  std::vector<float> uv(vertices * 2);
  std::vector<float> normalM(vertices * 3);
  std::vector<uint32_t> run(vertices);
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const float *const at = soup + vertex * kSoupFloatsPerVertex;
    positionsM[vertex * 3 + 0] = at[0];
    positionsM[vertex * 3 + 1] = at[1];
    positionsM[vertex * 3 + 2] = at[2];
    uv[vertex * 2 + 0] = at[3];
    uv[vertex * 2 + 1] = at[4];
    normalM[vertex * 3 + 0] = at[5];
    normalM[vertex * 3 + 1] = at[6];
    normalM[vertex * 3 + 2] = at[7];
    run[vertex] = static_cast<uint32_t>(vertex);
  }

  const int part = Held_.addPart(std::move(named), material);
  return Held_.setPositions(part, positionsM) && Held_.setTexture(part, uv) &&
         Held_.setNormals(part, normalM) && Held_.setTriangles(part, run);
}

} // namespace outshine::Generators
