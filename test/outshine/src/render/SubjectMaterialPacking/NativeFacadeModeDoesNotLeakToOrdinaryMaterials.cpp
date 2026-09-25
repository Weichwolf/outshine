#include <scene/Material.h>
#include "src/base/MaterialValidation.h"
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
  return Report();
}
