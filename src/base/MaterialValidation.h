#ifndef OUTSHINE_BASE_MATERIALVALIDATION_H
#define OUTSHINE_BASE_MATERIALVALIDATION_H
#include <scene/Material.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace outshine {
namespace MaterialValidation {
[[nodiscard]] inline bool Unit(float value) noexcept {
  return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

[[nodiscard]] inline bool Nonnegative(float value) noexcept {
  return std::isfinite(value) && value >= 0.0f;
}

[[nodiscard]] inline bool Map(const SurfaceMap &map, size_t images) noexcept {
  if (map.bound() && std::cmp_greater_equal(map.Image, images)) { return false; }
  if (map.Set != UvSet::Uv0 && map.Set != UvSet::Uv1) { return false; }
  const auto filter = [](Filter value) {
    return value == Filter::Nearest || value == Filter::Linear;
  };
  const auto wrap = [](Wrap value) {
    return value == Wrap::ClampToEdge || value == Wrap::MirroredRepeat || value == Wrap::Repeat;
  };
  const auto &sampler = map.Sampler;
  if (!filter(sampler.Magnify) || !filter(sampler.Minify) || !wrap(sampler.WrapU) ||
      !wrap(sampler.WrapV)) {
    return false;
  }
  if (sampler.Mip != MipFilter::None && sampler.Mip != MipFilter::Nearest &&
      sampler.Mip != MipFilter::Linear) {
    return false;
  }
  return std::isfinite(map.Uv.RotationRad) && std::isfinite(map.Uv.OffsetUv[0]) &&
         std::isfinite(map.Uv.OffsetUv[1]) && std::isfinite(map.Uv.ScaleUv[0]) &&
         std::isfinite(map.Uv.ScaleUv[1]);
}

[[nodiscard]] inline bool Factors(const Material &row) noexcept {
  for (const float value : {row.Metalness,
                            row.Roughness,
                            row.Transmission,
                            row.SpecularFactor,
                            row.SheenRoughness,
                            row.Clearcoat,
                            row.ClearcoatRoughness,
                            row.Anisotropy,
                            row.Iridescence}) {
    if (!Unit(value)) { return false; }
  }
  for (int channel = 0; channel < 3; ++channel) {
    if (!Unit(row.BaseColour[channel]) || !Unit(row.SpecularColour[channel]) ||
        !Unit(row.SheenColour[channel]) || !Unit(row.AttenuationColour[channel]) ||
        !Nonnegative(row.Emission[channel])) {
      return false;
    }
  }
  if (!Unit(row.BaseColour[3]) || !Nonnegative(row.CoverageCut) || !Nonnegative(row.Thickness)) {
    return false;
  }
  if (!std::isfinite(row.Ior) || (row.Ior != 0.0f && row.Ior < 1.0f)) { return false; }
  return std::isfinite(row.AnisotropyRotationRad) && std::isfinite(row.IridescenceIor) &&
         row.IridescenceIor >= 1.0f && Nonnegative(row.IridescenceThicknessMinNm) &&
         Nonnegative(row.IridescenceThicknessMaxNm) &&
         row.IridescenceThicknessMinNm <= row.IridescenceThicknessMaxNm &&
         row.AttenuationDistance > 0.0f;
}
}

[[nodiscard]] inline bool MaterialIsValid(const Material &row, size_t images) noexcept {
  if (!MaterialValidation::Factors(row)) { return false; }
  if (row.Alpha != AlphaMode::Opaque && row.Alpha != AlphaMode::Masked &&
      row.Alpha != AlphaMode::Blended) {
    return false;
  }
  const std::array maps{&row.BaseColourMap,
                        &row.NormalMap,
                        &row.MetalRoughMap,
                        &row.EmissiveMap,
                        &row.OcclusionMap,
                        &row.SpecularStrengthMap,
                        &row.SpecularTintMap};
  return std::ranges::all_of(
      maps, [images](const SurfaceMap *map) { return MaterialValidation::Map(*map, images); });
}
}
#endif
