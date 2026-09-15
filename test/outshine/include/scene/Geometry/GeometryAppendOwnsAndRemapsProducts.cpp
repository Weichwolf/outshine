#include <array>
#include <scene/Geometry.h>
#include "Check.h"

namespace {
outshine::Geometry Make(uint8_t red) {
  outshine::Geometry geometry;
  const std::array<uint8_t, 4> pixel{red, 0, 0, 255};
  const std::array<float, 9> positions{0, 0, 0, 1, 0, 0, 0, 1, 0};
  const std::array<uint32_t, 3> triangle{0, 1, 2};
  (void)geometry.addImage(1, 1, pixel);
  outshine::Material material;
  material.BaseColourMap.Image = 0;
  const auto surface = geometry.addSurface("surface", material);
  if (!surface) { return geometry; }
  const int part = geometry.addPart("triangle", *surface);
  (void)geometry.setPositions(part, positions);
  (void)geometry.setTriangles(part, triangle);
  return geometry;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry first = Make(17);
  Geometry second = Make(63);
  CHECK(first.wellFormed() && second.wellFormed(), "independent native products are complete");
  const auto combined = first.append(second);
  CHECK(combined.has_value() && first.parts() == 2 && first.surfaces() == 2 && first.images() == 2,
        "append publishes both complete products together");
  CHECK(first.materialOf(1).index() == 1 &&
            first.surfaceAt(MaterialInstance(1)).BaseColourMap.Image == 1 &&
            first.imageAt(1).Rgba[0] == 63,
        "part material and material image references are remapped into the new owner");
  CHECK(second.parts() == 1 && second.surfaces() == 1 && second.images() == 1 &&
            second.surfaceAt(MaterialInstance(0)).BaseColourMap.Image == 0 &&
            second.imageAt(0).Rgba[0] == 63,
        "append does not borrow or mutate the source product");
  Geometry incomplete;
  const int parts = first.parts();
  CHECK(first.append(incomplete).error() == GeometryAppendError::MalformedSource &&
            first.parts() == parts,
        "malformed source is rejected without publishing a partial combined product");
  return Report();
}
