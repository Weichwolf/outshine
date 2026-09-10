#include <scene/Geometry.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  Material previous;
  previous.Roughness = 0.25f;
  const auto index = geometry.addSurface("kept", previous).value();
  for (const bool missingImage : {false, true}) {
    auto invalid = previous;
    if (missingImage) {
      invalid.NormalMap.Image = 0;
    } else {
      invalid.Metalness = std::numeric_limits<float>::quiet_NaN();
    }
    const auto result = geometry.setSurface(index, invalid);
    CHECK(!result && result.error() == MaterialError::InvalidMaterial,
          "invalid replacement rejected with its typed error");
    CHECK(geometry.surfaceAt(index) == previous && geometry.surfaces() == 1 &&
              geometry.surfaceNameOf(0) == "kept",
          "failed replacement preserves values and identity");
  }
  const auto missing = geometry.setSurface(MaterialInstance(7), previous);
  CHECK(!missing && missing.error() == MaterialError::MissingMaterial,
        "missing slot distinguished");
  auto replacement = previous;
  replacement.Roughness = 0.75f;
  CHECK(static_cast<bool>(geometry.setSurface(index, replacement)), "valid retry succeeds");
  CHECK(geometry.surfaceAt(index) == replacement, "complete replacement visible");
  return Report();
}
