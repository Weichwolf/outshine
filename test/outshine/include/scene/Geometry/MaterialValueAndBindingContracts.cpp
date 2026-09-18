#include <scene/Geometry.h>

#include "Check.h"

#include <array>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Geometry geometry;
  Material valid;
  valid.Emission = {{1000.0f, 0.0f, 0.0f}};
  valid.Ior = 0.0f;
  valid.AttenuationDistance = std::numeric_limits<float>::infinity();
  const MaterialInstance surface = *geometry.addSurface("kept", valid);
  constexpr std::array<uint8_t, 4> pixels{255u, 255u, 255u, 255u};
  CHECK(*geometry.addImage(1, 1, pixels) == 0, "binding image exists");
  const int part = *geometry.addPart("triangle", surface);
  constexpr std::array positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  constexpr std::array indices{0u, 1u, 2u};
  CHECK(geometry.setPositions(part, positions) && geometry.setTriangles(part, indices),
        "material owner has complete geometry");

  const auto reject = [&](const Material &candidate) {
    const auto result = geometry.setSurface(surface, candidate);
    CHECK(!result && result.error() == MaterialError::InvalidMaterial,
          "invalid material replacement is rejected");
    CHECK(geometry.surfaceAt(surface) == valid, "rejection preserves the published material");
  };
  for (Vec3f Material::*member :
       {&Material::SpecularColour, &Material::SheenColour, &Material::AttenuationColour}) {
    for (const float value : {-0.1f,
                              1.1f,
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity()}) {
      Material candidate = valid;
      (candidate.*member)[1] = value;
      reject(candidate);
    }
  }
  for (const float value : {-0.1f,
                            1.1f,
                            std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity()}) {
    Material candidate = valid;
    candidate.OcclusionStrength = value;
    reject(candidate);
  }
  for (const float value :
       {-0.1f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
    Material candidate = valid;
    candidate.Emission[1] = value;
    reject(candidate);
    candidate = valid;
    candidate.CoverageCut = value;
    reject(candidate);
    candidate = valid;
    candidate.Thickness = value;
    reject(candidate);
  }
  for (const float value : {-1.0f,
                            0.5f,
                            std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity()}) {
    Material candidate = valid;
    candidate.Ior = value;
    reject(candidate);
    candidate = valid;
    candidate.IridescenceIor = value;
    reject(candidate);
  }
  for (const float value : {0.0f,
                            -1.0f,
                            std::numeric_limits<float>::quiet_NaN(),
                            -std::numeric_limits<float>::infinity()}) {
    Material candidate = valid;
    candidate.AttenuationDistance = value;
    reject(candidate);
  }
  for (const float value :
       {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
    Material candidate = valid;
    candidate.NormalScale = value;
    reject(candidate);
    candidate = valid;
    candidate.AnisotropyRotationRad = value;
    reject(candidate);
    candidate = valid;
    candidate.IridescenceThicknessMinNm = value;
    reject(candidate);
    candidate = valid;
    candidate.IridescenceThicknessMaxNm = value;
    reject(candidate);
  }
  Material invertedThickness = valid;
  invertedThickness.IridescenceThicknessMinNm = invertedThickness.IridescenceThicknessMaxNm + 1.0f;
  reject(invertedThickness);

  for (SurfaceMap Material::*member : {&Material::BaseColourMap,
                                       &Material::NormalMap,
                                       &Material::MetalRoughMap,
                                       &Material::EmissiveMap,
                                       &Material::OcclusionMap,
                                       &Material::SpecularStrengthMap,
                                       &Material::SpecularTintMap}) {
    Material candidate = valid;
    (candidate.*member).Image = 0;
    CHECK(geometry.setSurface(surface, candidate).has_value(), "every declared map binds an image");
    valid = candidate;

    candidate.*member = {};
    (candidate.*member).Set = static_cast<UvSet>(255);
    reject(candidate);
    candidate = valid;
    (candidate.*member).Sampler.Magnify = static_cast<Filter>(255);
    reject(candidate);
    candidate = valid;
    (candidate.*member).Sampler.Minify = static_cast<Filter>(255);
    reject(candidate);
    candidate = valid;
    (candidate.*member).Sampler.Mip = static_cast<MipFilter>(255);
    reject(candidate);
    candidate = valid;
    (candidate.*member).Sampler.WrapU = static_cast<Wrap>(255);
    reject(candidate);
    candidate = valid;
    (candidate.*member).Sampler.WrapV = static_cast<Wrap>(255);
    reject(candidate);
    candidate = valid;
    (candidate.*member).Uv.ScaleUv[0] = std::numeric_limits<float>::infinity();
    reject(candidate);
    candidate = valid;
    (candidate.*member).Image = 1;
    reject(candidate);
  }
  CHECK(geometry.wellFormed(), "every supported material binding remains owner-local and valid");
  return Report();
}
