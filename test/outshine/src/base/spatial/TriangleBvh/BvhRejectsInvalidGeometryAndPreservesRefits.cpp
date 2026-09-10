#include "TriangleBvh.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<float> positions;
  std::vector<uint32_t> indices;
  constexpr uint32_t count = 300;
  for (uint32_t at = 0; at < count; ++at) {
    const float x = static_cast<float>(at) * 2;
    positions.insert(positions.end(), {x, 0, 0, x + 1, 0, 0, x, 0, 1});
    indices.insert(indices.end(), {at * 3, at * 3 + 1, at * 3 + 2});
  }
  auto bvh = TriangleBvh::Over(positions, indices);
  CHECK(!bvh.Empty() && bvh.Nodes().size() > 1, "fixture exercises multiple BVH leaves");
  for (uint32_t at = 0; at < count; ++at) {
    const float x = static_cast<float>(at) * 2 + 0.25f;
    CHECK(bvh.Under(x, 0.25f) == 0.0f, "separated triangles match their analytic plane");
    CHECK(bvh.Occludes({.OriginM = {{x, 4, 0.25f}}, .Toward = {{0, -1, 0}}}, 0, 5),
          "each leaf remains reachable by its perpendicular ray");
    CHECK(!bvh.Occludes({.OriginM = {{x + 1, 4, 0.25f}}, .Toward = {{0, -1, 0}}}, 0, 5),
          "gaps between triangles remain unoccluded");
  }
  CHECK(bvh.Occludes({.OriginM = {{0, 4, 0.25f}}, .Toward = {{0, -1, 0}}}, 0, 5),
        "parallel slab ray on the box boundary still intersects the triangle edge");
  for (size_t at = 1; at < positions.size(); at += 3) { positions[at] = 2; }
  const BvhNode *nodes = bvh.Nodes().data();
  const BvhTriangle *triangles = bvh.Triangles().data();
  CHECK(bvh.Refit(positions) && bvh.Under(0.25f, 0.25f) == 2.0f,
        "refit moves the analytic plane and updates its bounds");
  CHECK(nodes == bvh.Nodes().data() && triangles == bvh.Triangles().data(),
        "refit preserves prepared storage addresses");
  auto invalid = positions;
  invalid[1] = 8;
  invalid.back() = std::numeric_limits<float>::quiet_NaN();
  CHECK(!bvh.Refit(invalid) && bvh.Under(0.25f, 0.25f) == 2.0f,
        "late invalid coordinates cannot publish an earlier changed triangle");
  CHECK(!bvh.Refit(std::span(positions).first(3)) && bvh.Under(0.25f, 0.25f) == 2.0f,
        "missing vertices preserve the accepted hierarchy");
  CHECK(TriangleBvh::Over(invalid, indices).Empty(), "nonfinite geometry is rejected at build");
  auto broken = indices;
  broken.back() = std::numeric_limits<uint32_t>::max();
  CHECK(TriangleBvh::Over(positions, broken).Empty(),
        "invalid indices cannot manufacture origin vertices");
  CHECK(TriangleBvh::Over(std::span(positions).first(positions.size() - 1), indices).Empty(),
        "incomplete position tuples are rejected");
  CHECK(!bvh.Occludes({.OriginM = {{0.25f, 4, 0.25f}}, .Toward = {}}, 0, 5),
        "a directionless ray cannot occlude");
  return Report();
}
