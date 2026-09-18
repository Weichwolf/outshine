#include <import/GltfImporter.h>

#include "Check.h"
#include "PreparedRoot.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  GltfImporter asset;
  const auto loaded = asset.load(PreparedRoot() + "/test-khronos-glTF-ChronographWatch/scene.gltf");
  CHECK(loaded.has_value(), "the pinned Khronos watch loads through the native importer");
  if (!loaded) { return Report(); }
  const Geometry &geometry = asset.geometry();
  constexpr int scaledMaterial = 22;
  CHECK(geometry.surfaces() > scaledMaterial, "the declared scaled normal material exists");
  if (geometry.surfaces() <= scaledMaterial) { return Report(); }
  const Material &material = geometry.surfaceAt(MaterialInstance(scaledMaterial));
  CHECK(material.NormalMap.bound() && material.NormalScale == 0.5f,
        "glTF normalTexture scale and image reach the native material together");
  return Report();
}
