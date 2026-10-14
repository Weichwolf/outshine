#include <scene/Material.h>
#include "MaterialValidation.h"
#include "SubjectMaterialPacking.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  SubjectMaterial ordinary;
  const auto flat = PackSubjectMaterial(ordinary, 0.0f);
  CHECK(ordinary.Row.Pattern == SurfacePattern::None && flat[35] == 0.0f,
        "ordinary and imported materials keep the unpatterned path");
  ordinary.Row.Pattern = SurfacePattern::Facade;
  const auto patterned = PackSubjectMaterial(ordinary, 0.0f);
  CHECK(MaterialValuesAreValid(ordinary.Row) && patterned[35] == 1.0f,
        "native facade mode reaches its dedicated GPU scalar");
  ordinary.Row.Pattern = static_cast<SurfacePattern>(2);
  CHECK(!MaterialValuesAreValid(ordinary.Row), "unknown procedural mode is rejected");
  Material material;
  material.Scattering = ScatteringModel::DiffuseThinSheet;
  material.BaseColour = {{.1f, .2f, .03f, 1}};
  material.DiffuseTransmission = {{.4f, .6f, .1f}};
  const auto packed = Render::PackSubjectMaterial({.Row = material}, 42);
  CHECK(packed[36] == 1 && packed[37] == .4f && packed[38] == .6f && packed[39] == .1f,
        "the CPU/GPU boundary carries the scattering model and direct RGB transmission");
  CHECK(packed[4] == .1f && packed[5] == .2f && packed[6] == .03f && packed[12] == 42,
        "reflection and surface identity preserve their existing packed interpretation");
  const auto defaultRow = Render::PackSubjectMaterial({}, 0);
  CHECK(defaultRow[36] == 0 && defaultRow[37] == 0 && defaultRow[38] == 0 && defaultRow[39] == 0,
        "existing metallic-roughness materials do not acquire diffuse transmission");
  SubjectMaterial mapped{.Row = material};
  mapped.Colour.Set = UvSet::Uv1;
  mapped.Colour.Uv.M[2] = 0.125;
  mapped.Colour.Uv.M[5] = 0.25;
  const auto mappedRow = PackSubjectMaterial(mapped, 0);
  CHECK(mappedRow[40] == 1 && mappedRow[42] == 0.125f && mappedRow[45] == 0.25f &&
            mappedRow[88] == 1 && mappedRow[89] == 0,
        "UV matrices and selectors follow the optical scalars without shifting their channels");
  mapped.Row.Scattering = ScatteringModel::AreaMixture;
  mapped.DiffuseReflection.Set = mapped.DiffuseTransmission.Set = UvSet::Uv1;
  mapped.DiffuseReflection.Uv.M[2] = 0.375;
  mapped.DiffuseTransmission.Uv.M[5] = 0.625;
  const auto mixedRow = PackSubjectMaterial(mapped, 0);
  CHECK(mixedRow[36] == 2 && mixedRow[78] == 0.375f && mixedRow[87] == 0.625f &&
            mixedRow[94] == 1 && mixedRow[95] == 1,
        "area-mixture optics have explicit UV transforms and selectors at the GPU boundary");
  CHECK(mapped.ReadsSecondUv() && mapped.ReadsAnyImage() == false,
        "optical UV selectors are accounted independently of image ownership");
  return Report();
}
