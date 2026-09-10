#include <scene/Geometry.h>
#include <Outshine.h>
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  Material valid;
  const auto surface = geometry.addSurface("material", valid);
  const int part = geometry.addPart("triangle", surface);
  constexpr std::array positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  constexpr std::array indices{0u, 1u, 2u};
  CHECK(geometry.setPositions(part, positions) && geometry.setTriangles(part, indices),
        "mesh builds");
  const auto assemble = [&](const Material &row) {
    geometry.clear();
    const auto slot = geometry.addSurface("candidate", row);
    const int triangle = geometry.addPart("triangle", slot);
    return geometry.setPositions(triangle, positions) && geometry.setTriangles(triangle, indices);
  };
  CHECK(geometry.wellFormed(), "default dielectric and infinite absorption distance are valid");
  for (const auto member : {&Material::Metalness,
                            &Material::Roughness,
                            &Material::Transmission,
                            &Material::SpecularFactor,
                            &Material::SheenRoughness,
                            &Material::Clearcoat,
                            &Material::ClearcoatRoughness,
                            &Material::Anisotropy,
                            &Material::Iridescence}) {
    for (const float value : {-0.1f,
                              1.1f,
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity()}) {
      auto bad = valid;
      bad.*member = value;
      CHECK(assemble(bad), "candidate values can be assembled");
      CHECK(!geometry.wellFormed(), "invalid unit-range factor cannot be published");
    }
  }
  auto bad = valid;
  bad.BaseColour[0] = -1.0f;
  CHECK(assemble(bad) && !geometry.wellFormed(), "negative reflectance rejected");
  bad = valid;
  bad.IridescenceThicknessMinNm = bad.IridescenceThicknessMaxNm + 1;
  CHECK(assemble(bad) && !geometry.wellFormed(), "reversed thickness rejected");
  bad = valid;
  bad.BaseColourMap.Image = 0;
  CHECK(assemble(bad) && !geometry.wellFormed(), "unresolved image rejected");
  const std::array<uint8_t, 4> pixel{255, 255, 255, 255};
  CHECK(geometry.addImage(1, 1, pixel) == 0 && geometry.wellFormed(),
        "resolved image permits publication");
  bad.BaseColourMap.Image = -1;
  bad.BaseColourMap.Sampler.Magnify = static_cast<Filter>(255);
  CHECK(assemble(bad) && !geometry.wellFormed(), "invalid sampler rejected");
  Engine engine;
  CHECK(!engine.setGeometry(geometry), "public engine rejects invalid material geometry");
  valid.Emission[0] = 1000;
  valid.Ior = 0;
  CHECK(geometry.setSurface(surface, valid) && geometry.wellFormed(),
        "HDR emission and disabled Fresnel valid");
  return Report();
}
