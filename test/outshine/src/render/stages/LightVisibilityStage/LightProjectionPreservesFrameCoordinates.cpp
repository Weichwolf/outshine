#include "LightVisibilityStage.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  LightVisibilityStage stage;
  stage.Declare({.ToSun = {{0, 0, 1}}, .Up = {{0, 1, 0}}}, 8.0);
  CHECK(stage.Standing(), "orthogonal finite light basis is declared");
  stage.Build({});
  const Mat4 original = stage.LightFromWorld();
  CHECK(original[0] == -0.125 && original[5] == 0.125 && original[10] == 0.03125 &&
            original[14] == 0.5 && original[15] == 1,
        "light projection has analytical axes and reverse depth");
  CHECK(stage.StoodAtM() == Vec3{}, "empty caster set stays at world origin");
  for (const Vec3 offset : std::array<Vec3, 3>{{{{4, -2, 8}}, {{-8, 4, -2}}, {{0, 0, 0}}}}) {
    stage.Build(offset);
    const auto &relative = stage.LightFromWorld();
    for (size_t row = 0; row < 3; ++row) {
      double projected = relative[12 + row];
      for (size_t axis = 0; axis < 3; ++axis) {
        projected += relative[axis * 4 + row] * offset[axis];
      }
      CHECK(projected == original[12 + row],
            "world origin represented in shifted coordinates keeps its light-space position");
    }
  }
  return Report();
}
