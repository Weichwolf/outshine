#include "SubjectMaterialPacking.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>

namespace outshine::Render {

PackedSubjectMaterial PackSubjectMaterial(const SubjectMaterial &material,
                                          float identity) noexcept {
  const Material &row = material.Row;
  Vec3f f0;
  DielectricF0(row, f0);
  const std::array scalars = {material.Coverage(),
                              material.State().CoverageCut(),
                              row.Metalness,
                              row.Roughness,
                              row.BaseColour[0],
                              row.BaseColour[1],
                              row.BaseColour[2],
                              row.BaseColour[3],
                              row.Emission[0],
                              row.Emission[1],
                              row.Emission[2],
                              material.NormalScale,
                              identity,
                              f0[0],
                              f0[1],
                              f0[2],
                              DielectricF90(row),
                              row.Transmission,
                              row.Thickness,
                              row.AttenuationDistance,
                              row.AttenuationColour[0],
                              row.AttenuationColour[1],
                              row.AttenuationColour[2],
                              row.SheenColour[0],
                              row.SheenColour[1],
                              row.SheenColour[2],
                              row.SheenRoughness,
                              row.Clearcoat,
                              row.ClearcoatRoughness,
                              row.Anisotropy,
                              row.AnisotropyRotationRad,
                              row.Iridescence,
                              row.IridescenceIor,
                              row.IridescenceThicknessMinNm,
                              row.IridescenceThicknessMaxNm,
                              static_cast<float>(row.Pattern)};
  static_assert(scalars.size() == kSubjectSurfaceScalars);
  PackedSubjectMaterial packed{};
  std::ranges::copy(scalars, packed.begin());
  const std::array<const SubjectTexture *const, kSubjectMaterialImages> images = {
      &material.Colour,
      &material.Normal,
      &material.MetalRough,
      &material.Emissive,
      &material.SpecularStrength,
      &material.SpecularTint};
  size_t at = kSubjectSurfaceScalars;
  for (const SubjectTexture *image : images) {
    for (const double element : image->Uv.M) { packed[at++] = static_cast<float>(element); }
  }
  for (const SubjectTexture *image : images) {
    packed[at++] = image->Set == UvSet::Uv1 ? 1.0f : 0.0f;
  }
  return packed;
}

}
