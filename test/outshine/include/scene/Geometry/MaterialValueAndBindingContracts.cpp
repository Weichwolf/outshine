#include <scene/Geometry.h>

#include "Check.h"

#include <array>
#include <cstdint>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckFactors(const Material &valid, const auto &reject) {
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
}

void CheckMaps(Material &valid, Geometry &geometry, MaterialInstance surface, const auto &reject) {
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
}

void CheckScattering(Material &valid,
                     Geometry &geometry,
                     MaterialInstance surface,
                     const auto &reject) {
  Material thin;
  thin.Scattering = ScatteringModel::DiffuseThinSheet;
  thin.BaseColour = {{0.1f, 0.2f, 0.03f, 1}};
  thin.DiffuseTransmission = {{0.4f, 0.6f, 0.1f}};
  thin.DoubleSided = true;
  CHECK(geometry.setSurface(surface, thin), "direct energy-bounded R/T publishes a native sheet");
  valid = thin;
  for (const float value : {-0.1f,
                            1.1f,
                            std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity()}) {
    auto invalid = thin;
    invalid.DiffuseTransmission[1] = value;
    reject(invalid);
  }
  auto invalid = thin;
  invalid.DiffuseTransmission = {{0.9f, 0.9f, 0.9f}};
  reject(invalid);
  invalid = thin;
  invalid.Scattering = ScatteringModel::MetallicRoughness;
  reject(invalid);
  invalid = thin;
  invalid.Scattering = static_cast<ScatteringModel>(255);
  reject(invalid);
  invalid = thin;
  invalid.Pattern = static_cast<SurfacePattern>(255);
  reject(invalid);
  invalid.Pattern = SurfacePattern::Facade;
  reject(invalid);
  for (float Material::*member : {&Material::Metalness,
                                  &Material::Transmission,
                                  &Material::Clearcoat,
                                  &Material::Anisotropy,
                                  &Material::Iridescence}) {
    invalid = thin;
    invalid.*member = 0.1f;
    reject(invalid);
  }
  invalid = thin;
  invalid.SheenColour[0] = 0.1f;
  reject(invalid);
  for (SurfaceMap Material::*member :
       {&Material::MetalRoughMap, &Material::SpecularStrengthMap, &Material::SpecularTintMap}) {
    invalid = thin;
    (invalid.*member).Image = 0;
    reject(invalid);
  }
  auto boundary = thin;
  boundary.BaseColour = {{0.25f, 0.25f, 0.25f, 1}};
  boundary.DiffuseTransmission = {{0.75f, 0.75f, 0.75f}};
  CHECK(geometry.setSurface(surface, boundary), "R/T permits exactly unit total energy");
  valid = boundary;
  CHECK(geometry.setSurface(surface, Material{}),
        "switching back resets thin transmission explicitly");
  valid = Material{};
}
}

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
    CHECK(geometry.surfaceAt(surface) == valid && geometry.surfaces() == 1 &&
              geometry.surfaceNameOf(0) == "kept",
          "rejection preserves material values, identity and name");
  };
  CheckFactors(valid, reject);
  const auto missing = geometry.setSurface(MaterialInstance(7), valid);
  CHECK(!missing && missing.error() == MaterialError::MissingMaterial,
        "missing slot is distinguished");
  auto replacement = valid;
  replacement.Roughness = 0.75f;
  CHECK(geometry.setSurface(surface, replacement), "a valid retry succeeds after rejected changes");
  CHECK(geometry.surfaceAt(surface) == replacement, "the complete replacement is visible");
  valid = replacement;
  CheckMaps(valid, geometry, surface, reject);
  CheckScattering(valid, geometry, surface, reject);
  Material mixture;
  mixture.Scattering = ScatteringModel::AreaMixture;
  mixture.Alpha = AlphaMode::Masked;
  mixture.DiffuseTransmission = {{1, 1, 1}};
  mixture.BaseColourMap.Image = mixture.DiffuseReflectionMap.Image =
      mixture.DiffuseTransmissionMap.Image = 0;
  CHECK(geometry.setSurface(surface, mixture),
        "a bounded optical area mixture binds native images");
  valid = mixture;
  for (SurfaceMap Material::*member : {&Material::BaseColourMap,
                                       &Material::DiffuseReflectionMap,
                                       &Material::DiffuseTransmissionMap}) {
    auto invalid = mixture;
    (invalid.*member).Image = -1;
    reject(invalid);
    invalid = mixture;
    (invalid.*member).Image = 1;
    reject(invalid);
  }
  auto invalid = mixture;
  invalid.Alpha = AlphaMode::Opaque;
  reject(invalid);
  invalid = mixture;
  invalid.Transmission = 0.1f;
  reject(invalid);
  CHECK(geometry.setSurface(surface, Material{}),
        "mixture maps do not leak into an ordinary material");
  valid = Material{};
  CHECK(geometry.wellFormed(), "every supported material binding remains owner-local and valid");
  return Report();
}
