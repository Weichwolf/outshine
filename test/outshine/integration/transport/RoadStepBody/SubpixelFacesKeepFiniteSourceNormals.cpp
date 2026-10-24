#include "Check.h"
#include "RoadStepBody.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double eastM = 100000;
  constexpr double widthM = .001;
  constexpr std::array points{PlanarPoint{eastM, 0},
                              PlanarPoint{eastM + widthM, 0},
                              PlanarPoint{eastM + widthM, 1},
                              PlanarPoint{eastM, 1}};
  constexpr std::array faces{PlanarFace{0, 1, 2}, PlanarFace{0, 2, 3}};
  constexpr std::array<std::array<double, 3>, 2> tops{};
  constexpr std::array bottoms{-.25, -.25, -.25, -.25};
  auto solid = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = tops, .BottomHeights = bottoms});
  CHECK(solid.has_value(), "a narrow source body remains valid before render quantization");
  if (!solid) { return Report(); }
  CHECK(static_cast<float>(eastM) == static_cast<float>(eastM + widthM),
        "Float32 merges the two source sides at this render-coordinate offset");
  SurfaceBody body;
  body.Geometry = std::move(*solid);
  RoadMeshBuffers mesh;
  CHECK(AppendRoadStepBody(body, {{1, 1, 1}}, mesh), "the valid body reaches render buffers");
  CHECK(!mesh.NormalM.empty() &&
            std::ranges::all_of(mesh.NormalM, [](float normal) { return std::isfinite(normal); }),
        "subpixel triangles retain finite normals from their unquantized source body");
  return Report();
}
