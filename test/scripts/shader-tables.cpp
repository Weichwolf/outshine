#include "SheenLobe.h"
#include "MicrofacetEnergy.h"
#include "IridescenceLobe.h"
#include <cstdio>

using namespace outshine::Render;

int main() {
  std::printf("const int kSheenSteps = %d;\nconst float kSheenAlbedo[] = float[](\n",
              kSheenAlbedoSteps);
  for (int r = 0; r < kSheenAlbedoSteps; ++r) {
    for (int v = 0; v < kSheenAlbedoSteps; ++v) {
      std::printf("%s%.6f",
                  r == 0 && v == 0 ? "" : ",",
                  SheenDirectionalAlbedo({.Cosine = (v + 0.5) / kSheenAlbedoSteps,
                                          .Roughness = (r + 0.5) / kSheenAlbedoSteps}));
    }
  }
  std::printf(");\nconst int kEnergyRoughnessSteps = %d;\nconst int kEnergyViewSteps = %d;\nconst "
              "float kGgxAlbedo[] = float[](\n",
              kEnergyRoughnessSteps,
              kEnergyViewSteps);
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    for (int v = 0; v < kEnergyViewSteps; ++v) {
      std::printf("%s%.6f",
                  r == 0 && v == 0 ? "" : ",",
                  GgxDirectionalAlbedo(
                      {.Cosine = static_cast<double>(v) / (kEnergyViewSteps - 1),
                       .Roughness = static_cast<double>(r) / (kEnergyRoughnessSteps - 1)}));
    }
  }
  std::printf(");\nconst float kGgxAlbedoAverage[] = float[](\n");
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    std::printf("%s%.6f",
                r == 0 ? "" : ",",
                GgxEnergyAverage(static_cast<double>(r) / (kEnergyRoughnessSteps - 1)));
  }
  std::printf(");\nconst float kIriOutsideIor = %.17g;\nconst float kIriF0Ceiling = %.17g;\n",
              kOutsideIor,
              kFresnelInverseCeiling);
  const auto vector = [](const char *name, const std::array<double, 3> &value) {
    std::printf("const vec3 %s = vec3(%.6g, %.6g, %.6g);\n", name, value[0], value[1], value[2]);
  };
  vector("kIriVal", kSensitivityVal);
  vector("kIriPos", kSensitivityPos);
  vector("kIriVar", kSensitivityVar);
  std::printf("const float kIriValX2 = %.6g;\nconst float kIriPosX2 = %.6g;\nconst float kIriVarX2 "
              "= %.6g;\nconst float kIriNorm = %.6g;\n",
              kSensitivityValX2,
              kSensitivityPosX2,
              kSensitivityVarX2,
              kSensitivityNorm);
  std::printf("const mat3 kIriXyzToRgb = mat3(");
  for (size_t col = 0; col < 3; ++col) {
    for (size_t row = 0; row < 3; ++row) {
      std::printf("%s%.8g", row == 0 && col == 0 ? "" : ",", kXyzToRec709[row][col]);
    }
  }
  std::printf(");\n");
  return std::ferror(stdout) != 0 ? 1 : 0;
}
