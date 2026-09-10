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
  const auto surface = geometry.addSurface("material", valid).value();
  const int part = geometry.addPart("triangle", surface);
  constexpr std::array positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  constexpr std::array indices{0u, 1u, 2u};
  CHECK(geometry.setPositions(part, positions) && geometry.setTriangles(part, indices),
        "mesh builds");
  CHECK(geometry.wellFormed(), "default dielectric and infinite absorption distance are valid");
  const auto reject = [&](const Material &row) {
    const auto added = geometry.addSurface("rejected", row);
    CHECK(!added && added.error() == MaterialError::InvalidMaterial,
          "invalid values fail at material creation");
    CHECK(geometry.surfaces() == 1 && geometry.surfaceNameOf(0) == "material" &&
              geometry.surfaceAt(surface) == valid && geometry.wellFormed(),
          "failure preserves the published material and geometry");
  };
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
      reject(bad);
    }
  }
  auto bad = valid;
  bad.BaseColour[0] = -1.0f;
  reject(bad);
  bad = valid;
  bad.IridescenceThicknessMinNm = bad.IridescenceThicknessMaxNm + 1;
  reject(bad);
  bad = valid;
  bad.BaseColourMap.Sampler.Magnify = static_cast<Filter>(255);
  reject(bad);
  bad = valid;
  bad.Alpha = static_cast<AlphaMode>(255);
  reject(bad);
  bad = valid;
  bad.NormalMap.Uv.OffsetUv[0] = std::numeric_limits<float>::quiet_NaN();
  reject(bad);
  bad = valid;
  bad.BaseColourMap.Image = 0;
  const auto forward = geometry.addSurface("forward", bad).value();
  CHECK(forward.index() == 1 && geometry.setMaterial(part, forward),
        "failed additions consume no slots; forward image references can be assembled");
  CHECK(!geometry.wellFormed(), "unresolved image prevents publication");
  Engine engine;
  CHECK(!engine.setGeometry(geometry), "public engine rejects unresolved material geometry");
  const std::array<uint8_t, 4> pixel{255, 255, 255, 255};
  CHECK(geometry.addImage(1, 1, pixel) == 0 && geometry.wellFormed(),
        "resolved image permits publication");
  valid.Emission[0] = 1000;
  valid.Ior = 0;
  CHECK(geometry.setSurface(surface, valid) && geometry.wellFormed(),
        "HDR emission and disabled Fresnel valid");
  return Report();
}
