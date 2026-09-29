#include "StructureSurfaceIndex.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  Raised mesh;
  const std::array<std::array<Vec3f, 3>, 5> triangles{
      {{{{{-4, -3, -2}}, {{-2, -3, -2}}, {{-3, -1, -2}}}},
       {{{{0, 0, 0}}, {{2, 0, 0}}, {{0, 2, 0}}}},
       {{{{10, 8, 9}}, {{8, 8, 9}}, {{10, 7, 9}}}},
       {{{{4, 4, 4}}, {{4, 4, 4}}, {{4, 4, 4}}}},
       {{{{1, 5, 2}}, {{3, 5, 2}}, {{2, 6, 2}}}}}};
  for (size_t triangle = 0; triangle < triangles.size(); ++triangle) {
    auto &indices = triangle < 2 ? mesh.WallRun : mesh.RoofRun;
    auto &vertices = triangle < 2 ? mesh.WallCorners : mesh.RoofCorners;
    for (const auto point : triangles[triangle]) {
      indices.push_back(static_cast<uint32_t>(vertices.size()));
      vertices.push_back({.pos = point});
    }
  }
  StructureSurfaceIndex index;
  index.Reset(mesh);
  bool finished = false;
  for (size_t step = 0; step < 72 * triangles.size(); ++step) {
    if (index.Step()) {
      finished = true;
      break;
    }
  }
  CHECK(finished, "index construction and capped matching have bounded steps");
  CHECK(index.At(0).Count == triangles.size(), "root includes every wall and roof triangle");
  std::array<uint32_t, 16> pending{};
  size_t count = 1;
  size_t leaves = 0;
  bool coverage = true;
  while (count > 0) {
    const auto &node = index.At(pending[--count]);
    for (size_t triangle = node.First; triangle < node.First + node.Count; ++triangle) {
      for (const auto point : triangles[triangle]) {
        for (size_t axis = 0; axis < 3; ++axis) {
          coverage &= node.MinM[axis] <= point[axis] && point[axis] <= node.MaxM[axis];
        }
      }
    }
    if (node.Count == 1) {
      const auto actual = index.Triangle(node.First);
      for (size_t corner = 0; corner < 3; ++corner) {
        for (size_t axis = 0; axis < 3; ++axis) {
          coverage &= actual[corner].EstimateM[axis] == triangles[node.First][corner][axis];
        }
      }
      ++leaves;
    } else {
      pending[count++] = node.Left;
      pending[count++] = node.Right;
    }
  }
  CHECK(coverage && leaves == triangles.size(),
        "every ancestor encloses its original native vertices");
  for (const auto point : {Vec3{{13, 12, 9}}, Vec3{{-7, -7, -2}}}) {
    const double lower = index.LowerDistance(0, point);
    CHECK(lower <= 5 && lower >= 5 - 1e-12,
          "3-4-5 separation rounds toward a conservative lower bound");
  }
  CHECK(index.LowerDistance(0, {{0, 0, 0}}) == 0, "inside-box distance is zero");
  return Report();
}
