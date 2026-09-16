#include <scene/Geometry.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const auto first = geometry.addSurface("first", {}).value();
  const auto second = geometry.addSurface("second", {}).value();
  const int part = geometry.addPart("part", first);
  Mat4 placement;
  placement.SetTranslation({{3, 5, 7}});
  const auto created = geometry.addLamp("lamp", {}, placement);
  CHECK(created.has_value(), "native light creation returns an owned result");
  if (!created) { return Report(); }
  const int lamp = *created;
  PunctualLight light;
  light.Intensity = 23;
  CHECK(geometry.setPlacement(part, placement).has_value(), "direct placement mutation succeeds");
  CHECK(geometry.setMaterial(part, second).has_value(), "direct material mutation succeeds");
  CHECK(geometry.setLight(lamp, light).has_value(), "direct light mutation succeeds");
  for (int absent : {-1, 1}) {
    CHECK(!geometry.setPlacement(absent, {}), "absent part rejects placement");
    CHECK(geometry.setMaterial(absent, first).error() == MaterialBindingError::MissingPart,
          "absent part diagnoses material binding");
    CHECK(geometry.setLight(absent, {}).error() == LightMutationError::MissingLight,
          "absent lamp diagnoses light mutation");
  }
  CHECK(geometry.setMaterial(part, {}).error() == MaterialBindingError::MissingMaterial,
        "unbound material is diagnosed");
  CHECK(geometry.setMaterial(part, MaterialInstance(geometry.surfaces())).error() ==
            MaterialBindingError::MissingMaterial,
        "out-of-range material is diagnosed");
  CHECK(geometry.materialOf(part) == second, "rejected changes retain the assigned material");
  CHECK(geometry.placementOf(part)[12] == 3 && geometry.placementOf(part)[13] == 5 &&
            geometry.placementOf(part)[14] == 7,
        "material changes leave the part placement intact");
  CHECK(geometry.lampAt(lamp).Intensity == 23 && geometry.lampNameOf(lamp) == "lamp" &&
            geometry.lampPlacementOf(lamp)[12] == 3,
        "light edits preserve name and placement");
  CHECK(geometry.nameOf(part) == "part" && geometry.surfaceNameOf(first.index()) == "first" &&
            geometry.surfaceNameOf(second.index()) == "second",
        "mutations do not replace owned names or material entries");
  geometry.clear();
  CHECK(!geometry.setPlacement(part, placement) && !geometry.setMaterial(part, second) &&
            !geometry.setLight(lamp, light),
        "retained storage after clear is not active content");
  return Report();
}
