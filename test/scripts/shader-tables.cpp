#include "SheenLobe.h"
#include "MicrofacetEnergy.h"
#include "IridescenceLobe.h"
#include <array>
#include <cstdio>
#include <print>
#include <string_view>

using namespace outshine::Render;

namespace {
struct MatrixShape {
  int Rows;
  int Columns;
};

void Matrix(std::string_view name, MatrixShape shape, auto value) {
  std::print("const float {}[] = float[](\n", name);
  for (int row = 0; row < shape.Rows; ++row) {
    for (int column = 0; column < shape.Columns; ++column) {
      std::print("{}{:.6f}", row == 0 && column == 0 ? "" : ",", value(row, column));
    }
  }
  std::print(");\n");
}

Slant EnergySample(int roughness, int view) {
  return {.Cosine = static_cast<double>(view) / (kEnergyViewSteps - 1),
          .Roughness = static_cast<double>(roughness) / (kEnergyRoughnessSteps - 1)};
}

void EnergyTables() {
  std::print("const int kEnergyRoughnessSteps = {};\nconst int kEnergyViewSteps = {};\n",
             kEnergyRoughnessSteps,
             kEnergyViewSteps);
  Matrix("kGgxAlbedo",
         {.Rows = kEnergyRoughnessSteps, .Columns = kEnergyViewSteps},
         [](int r, int v) { return GgxEnvironmentBrdf(EnergySample(r, v)).Albedo; });
  Matrix("kGgxFresnelBias",
         {.Rows = kEnergyRoughnessSteps, .Columns = kEnergyViewSteps},
         [](int r, int v) { return GgxEnvironmentBrdf(EnergySample(r, v)).FresnelBias; });
  Matrix("kGgxAlbedoAverage", {.Rows = kEnergyRoughnessSteps, .Columns = 1}, [](int r, int) {
    return GgxEnergyAverage(EnergySample(r, 0).Roughness);
  });
}

void IridescenceTables() {
  std::print("const float kIriOutsideIor = {:.17g};\nconst float kIriF0Ceiling = {:.17g};\n",
             kOutsideIor,
             kFresnelInverseCeiling);
  const auto vector = [](std::string_view name, const std::array<double, 3> &value) {
    std::print(
        "const vec3 {} = vec3({:.6g}, {:.6g}, {:.6g});\n", name, value[0], value[1], value[2]);
  };
  vector("kIriVal", kSensitivityVal);
  vector("kIriPos", kSensitivityPos);
  vector("kIriVar", kSensitivityVar);
  std::print("const float kIriValX2 = {:.6g};\nconst float kIriPosX2 = {:.6g};\nconst float "
             "kIriVarX2 = {:.6g};\nconst float kIriNorm = {:.6g};\n",
             kSensitivityValX2,
             kSensitivityPosX2,
             kSensitivityVarX2,
             kSensitivityNorm);
  std::print("const mat3 kIriXyzToRgb = mat3(");
  for (size_t col = 0; col < 3; ++col) {
    for (size_t row = 0; row < 3; ++row) {
      std::print("{}{:.8g}", row == 0 && col == 0 ? "" : ",", kXyzToRec709[row][col]);
    }
  }
  std::print(");\n");
}
}

int main() try {
  std::print("const int kSheenSteps = {};\n", kSheenAlbedoSteps);
  Matrix(
      "kSheenAlbedo", {.Rows = kSheenAlbedoSteps, .Columns = kSheenAlbedoSteps}, [](int r, int v) {
        return SheenDirectionalAlbedo(
            {.Cosine = (v + 0.5) / kSheenAlbedoSteps, .Roughness = (r + 0.5) / kSheenAlbedoSteps});
      });
  EnergyTables();
  IridescenceTables();
  return std::ferror(stdout) != 0 ? 1 : 0;
} catch (...) {
  std::fputs("shader table generation failed\n", stderr);
  return 1;
}
