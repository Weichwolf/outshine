#include "Check.h"
#include "RoadBodyMesh.h"
#include <algorithm>
#include <array>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array<PlanarPoint, 3> points{
      {{0, 0}, {100, -99.9799957275390625}, {99.98999786376953125, -99.970001220703125}}};
  constexpr std::array heights{0., 99.98999786376953125, 99.9799957275390625};
  constexpr std::array<PlanarFace, 1> faces{{{0, 2, 1}}};
  constexpr std::array<std::array<double, 3>, 1> tops{{{heights[0], heights[2], heights[1]}}};
  constexpr double thicknessM = .25;
  constexpr std::array bottoms{
      heights[0] - thicknessM, heights[1] - thicknessM, heights[2] - thicknessM};
  auto solid = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = tops, .BottomHeights = bottoms});
  CHECK(solid.has_value(), "the thin indexed solid is closed and representable in Float32");
  if (!solid) { return Report(); }
  RoadMeshBuffers mesh;
  CHECK(AppendRoadBodyMesh(*solid, {{1, 1, 1}}, mesh), "the validated body reaches render buffers");
  CHECK(!mesh.NormalM.empty() &&
            std::ranges::all_of(mesh.NormalM, [](float normal) { return std::isfinite(normal); }),
        "normal construction retains the nonzero cross product of representable thin faces");
  return Report();
}
