#include <scene/Geometry.h>
#include "Subject.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry input;
  const auto first = input.addSurface("valid", Material{});
  Material invalid;
  invalid.Roughness = std::numeric_limits<float>::quiet_NaN();
  const auto last = input.addSurface("invalid", invalid);
  constexpr std::array positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  constexpr std::array indices{0u, 1u, 2u};
  for (const auto material : {first, last}) {
    const int part = input.addPart("triangle", material);
    CHECK(input.setPositions(part, positions) && input.setTriangles(part, indices),
          "source mesh prepared");
  }
  Gltf::Subject source;
  CHECK(source.Assemble(input),
        "legacy source can hold a candidate awaiting publication validation");
  const auto failed = source.Handed();
  CHECK(!failed && !failed.error().empty(),
        "invalid later material returns an error, not a partial product");
  CHECK(source.Parts().size() == 2, "failed conversion retains its source");
  CHECK(input.setSurface(last, Material{}).has_value() && source.Assemble(input),
        "corrected source prepared");
  auto complete = source.Handed();
  CHECK(complete && complete->parts() == 2 && complete->surfaces() == 2 && complete->wellFormed(),
        "valid retry returns a complete native product");
  return Report();
}
