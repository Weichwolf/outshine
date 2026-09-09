#include "AudioOcclusion.h"
#include "Check.h"
#include "scene/Geometry.h"
#include <array>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const std::array<float, 9> wall{-1, -1, 0, 1, -1, 0, 0, 1, 0};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  const int material = geometry.addSurface("wall", {}).index();
  for (int n = 0; n < 2; ++n) {
    const int part = geometry.addPart("wall", MaterialInstance(material));
    CHECK(geometry.setPositions(part, wall), "native wall positions");
    CHECK(geometry.setTriangles(part, indices), "native wall triangles");
    Mat4 placement;
    placement[0] = n == 0 ? 2.0 : -2.0;
    placement.SetTranslation({{static_cast<double>(n * 4), 0, 5}});
    CHECK(geometry.setPlacement(part, placement), "placed and reflected walls");
  }
  const std::array<float, 9> ground{7, -1, 3, 9, -1, 3, 8, 1, 3};
  auto built = Core::BuildAudioOcclusion(geometry, ground, indices);
  CHECK(built.has_value(), "native geometry and supplemental ground build without a renderer");
  if (!built) { return Report(); }
  CHECK(built->Triangles().size() == 3, "both parts and ground are retained");
  for (float x : {0.0f, 4.0f, 8.0f}) {
    CHECK(built->Occludes({{{x, 0, 0}}, {{0, 0, 1}}}, 0.01f, 10),
          "placed, reflected and rebased supplemental triangles occlude");
  }
  CHECK(!built->Occludes({{{0, 0, 0}}, {{0, 0, 1}}}, 0.01f, 4),
        "wall beyond source does not occlude");
  CHECK(!built->Occludes({{{20, 0, 0}}, {{0, 0, 1}}}, 0.01f, 10),
        "ray missing geometry remains unobstructed");
  CHECK(built->Occludes({{{4, 0, 10}}, {{0, 0, -1}}}, 0.01f, 10),
        "occlusion is two sided after reflection");
  geometry.clear();
  CHECK(built->Occludes({{{0, 0, 0}}, {{0, 0, 1}}}, 0.01f, 10),
        "BVH owns its triangles after source clear");
  CHECK(!Core::BuildAudioOcclusion(geometry, ground, std::array<uint32_t, 3>{0, 1, 3}),
        "invalid vertex index is rejected instead of replaced by origin");
  CHECK(!Core::BuildAudioOcclusion(geometry, std::array<float, 2>{0, 0}, indices),
        "partial position is rejected");
  CHECK(!Core::BuildAudioOcclusion(geometry, ground, std::array<uint32_t, 2>{0, 1}),
        "partial triangle is rejected");
  auto invalid = ground;
  invalid[0] = std::numeric_limits<float>::infinity();
  CHECK(!Core::BuildAudioOcclusion(geometry, invalid, indices), "nonfinite ground is rejected");
  const auto empty = Core::BuildAudioOcclusion(geometry);
  CHECK(empty && empty->Empty(), "empty scene has no occluders");
  return Report();
}
