#ifndef OUTSHINE_RENDER_STAGES_IRIDESCENCELOBE_H
#define OUTSHINE_RENDER_STAGES_IRIDESCENCELOBE_H

#include <array>
#include <cstdio>
#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

inline constexpr double kOutsideIor = 1.0;
inline constexpr double kFresnelInverseCeiling = 0.9999;

inline constexpr std::array<double, 3> kSensitivityVal = {5.4856e-13, 4.4201e-13, 5.2481e-13};
inline constexpr std::array<double, 3> kSensitivityPos = {1.6810e+06, 1.7953e+06, 2.2084e+06};
inline constexpr std::array<double, 3> kSensitivityVar = {4.3278e+09, 9.3046e+09, 6.6121e+09};
inline constexpr double kSensitivityValX2 = 9.7470e-14;
inline constexpr double kSensitivityPosX2 = 2.2399e+06;
inline constexpr double kSensitivityVarX2 = 4.5282e+09;
inline constexpr double kSensitivityNorm = 1.0685e-7;

inline constexpr std::array<std::array<double, 3>, 3> kXyzToRec709 = {{
    {3.2404542, -1.5371385, -0.4985314},
    {-0.9692660, 1.8760108, 0.0415560},
    {0.0556434, -0.2040259, 1.0572252},
}};

inline ShaderText &IridescenceLobe(ShaderText &into) {
  std::array<char, 1024> constants{};
  std::snprintf(constants.data(),
                constants.size(),
                "constant float kIriOutsideIor = %.17g;\n"
                "constant float kIriF0Ceiling = %.17g;\n"
                "constant float3 kIriVal = float3(%.6g, %.6g, %.6g);\n"
                "constant float3 kIriPos = float3(%.6g, %.6g, %.6g);\n"
                "constant float3 kIriVar = float3(%.6g, %.6g, %.6g);\n"
                "constant float kIriValX2 = %.6g;\n"
                "constant float kIriPosX2 = %.6g;\n"
                "constant float kIriVarX2 = %.6g;\n"
                "constant float kIriNorm = %.6g;\n"
                "constant float3x3 kIriXyzToRgb = float3x3(float3(%.8g, %.8g, %.8g),"
                " float3(%.8g, %.8g, %.8g), float3(%.8g, %.8g, %.8g));\n",
                kOutsideIor,
                kFresnelInverseCeiling,
                kSensitivityVal[0],
                kSensitivityVal[1],
                kSensitivityVal[2],
                kSensitivityPos[0],
                kSensitivityPos[1],
                kSensitivityPos[2],
                kSensitivityVar[0],
                kSensitivityVar[1],
                kSensitivityVar[2],
                kSensitivityValX2,
                kSensitivityPosX2,
                kSensitivityVarX2,
                kSensitivityNorm,

                kXyzToRec709[0][0],
                kXyzToRec709[1][0],
                kXyzToRec709[2][0],
                kXyzToRec709[0][1],
                kXyzToRec709[1][1],
                kXyzToRec709[2][1],
                kXyzToRec709[0][2],
                kXyzToRec709[1][2],
                kXyzToRec709[2][2]);
  return into.Adds(constants.data()).Reads("src/render/shaders/iridescenceLobe.msl");
}

[[nodiscard]] inline std::string IridescenceLobeMsl(std::string &error) {
  ShaderText source;
  return IridescenceLobe(source).Take(error);
}

[[nodiscard]] inline std::string IridescenceLobeMsl() {
  std::string ignored;
  return IridescenceLobeMsl(ignored);
}

} // namespace outshine::Render

#endif
