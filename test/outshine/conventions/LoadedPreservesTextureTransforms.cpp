#include <array>
#include <numbers>
#include <scene/Loaded.h>
#include "Check.h"
#include "PreparedRoot.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Loaded asset;
  const auto loaded =
      asset.load(PreparedRoot() + "/test-khronos-glTF-TextureTransformTest/scene.gltf");
  CHECK(loaded.has_value(),
        "the pinned Khronos TextureTransformTest loads through the public door");
  if (!loaded) { return Report(); }
  const std::array<UvTransformProperties, 6> expected = {{
      {.OffsetUv = {{0.5, 0}}},
      {.OffsetUv = {{0, 0.5}}},
      {.OffsetUv = {{0.5, 0.5}}},
      {.RotationRad = -std::numbers::pi / 8},
      {.ScaleUv = {{1.5, 1.5}}},
      {.OffsetUv = {{-0.2, -0.1}}, .RotationRad = -0.3, .ScaleUv = {{1.5, 1.5}}},
  }};
  const Geometry &geometry = asset.geometry();
  CHECK(geometry.surfaces() >= static_cast<int>(expected.size()),
        "the six transformed Khronos materials are exposed");
  for (size_t at = 0; at < expected.size() && at < static_cast<size_t>(geometry.surfaces()); ++at) {
    const SurfaceMap &map =
        geometry.surfaceAt(MaterialInstance(static_cast<int>(at))).BaseColourMap;
    CHECK(map.bound(), "the material exposes its image");
    CHECK(map.Uv == expected[at],
          "glTF rotation is converted to the native algebraic UV convention; offset and scale "
          "survive");
  }
  return Report();
}
