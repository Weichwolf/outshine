#ifndef OUTSHINE_RENDER_STAGES_IRIDESCENCELOBE_H
#define OUTSHINE_RENDER_STAGES_IRIDESCENCELOBE_H

#include <array>

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

}

#endif
