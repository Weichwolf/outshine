#include <array>
#include <scene/Geometry.h>
#include <scene/Material.h>
#include "Check.h"
#include "Subject.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  Material material;
  material.NeedsTangents = true;
  const auto surface = geometry.addSurface("normal mapped", material);
  CHECK(surface.has_value(), "material requests tangent space");
  if (!surface) { return Report(); }
  constexpr std::array positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  constexpr std::array normals{0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
  for (const float direction : {1.0f, -1.0f}) {
    const int part = geometry.addPart("triangle", *surface);
    const std::array uv{0.0f, 0.0f, direction, 0.0f, 0.0f, 1.0f};
    CHECK(geometry.setPositions(part, positions) && geometry.setNormals(part, normals) &&
              geometry.setTexture(part, uv) && geometry.setTriangles(part, std::array{0u, 1u, 2u}),
          "native triangle attributes are accepted");
  }
  Gltf::Subject subject;
  CHECK(subject.Assemble(geometry), "native material assembles through tangent generation");
  CHECK(subject.Parts().size() == 2 && subject.Tangents().size() == 24,
        "both primitives publish their own tangent ranges");
  if (subject.Parts().size() != 2 || subject.Tangents().size() != 24) { return Report(); }
  for (size_t part = 0; part < 2; ++part) {
    CHECK(subject.Parts()[part].Tangent == Gltf::TangentSource::Generated,
          "basis came from the generator, not supplied data");
    const double direction = part == 0 ? 1 : -1;
    for (size_t corner = 0; corner < 3; ++corner) {
      const size_t at = (subject.Parts()[part].FirstVertex + corner) * 4;
      CHECK_NEAR(subject.Tangents()[at],
                 direction,
                 1e-12,
                 "unit",
                 "each primitive retains its U direction");
      CHECK(subject.Tangents()[at + 3] == -direction, "each primitive retains its handedness");
    }
  }
  return Report();
}
