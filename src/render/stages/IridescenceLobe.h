#ifndef IRIDESCENCELOBE_H
#define IRIDESCENCELOBE_H

#include "MetalRoughBrdf.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace outshine::Render {

inline constexpr double kOutsideIor = 1.0;

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

[[nodiscard]] inline double IorToFresnel0(double transmitted, double incident) {
  const double d = (transmitted - incident) / (transmitted + incident);
  return d * d;
}

inline constexpr double kFresnelInverseCeiling = 0.9999;

[[nodiscard]] inline double Fresnel0ToIor(double f0) {
  const double s = std::sqrt(std::fmin(std::fmax(f0, 0.0), kFresnelInverseCeiling));
  return (1.0 + s) / (1.0 - s);
}

[[nodiscard]] inline double IridescenceSchlick(double f0, double cosTheta) {
  const double m = 1.0 - cosTheta;
  const double m2 = m * m;
  return f0 + (1.0 - f0) * m2 * m2 * m;
}

inline void IridescenceSensitivity(double opdNm, const std::array<double, 3> &shift,
                                   std::array<double, 3> &rgb) {
  const double phase = 2.0 * kBrdfPi * opdNm * 1.0e-9;
  std::array<double, 3> xyz{};
  for (int c = 0; c < 3; ++c) {
    xyz[static_cast<std::size_t>(c)] =
        kSensitivityVal[static_cast<std::size_t>(c)] *
        std::sqrt(2.0 * kBrdfPi * kSensitivityVar[static_cast<std::size_t>(c)]) *
        std::cos(kSensitivityPos[static_cast<std::size_t>(c)] * phase +
                 shift[static_cast<std::size_t>(c)]) *
        std::exp(-phase * phase * kSensitivityVar[static_cast<std::size_t>(c)]);
  }
  xyz[0] += kSensitivityValX2 * std::sqrt(2.0 * kBrdfPi * kSensitivityVarX2) *
            std::cos(kSensitivityPosX2 * phase + shift[0]) *
            std::exp(-kSensitivityVarX2 * phase * phase);
  for (double &v : xyz) { v /= kSensitivityNorm; }
  for (int c = 0; c < 3; ++c) {
    const std::array<double, 3> &row = kXyzToRec709[static_cast<std::size_t>(c)];
    rgb[static_cast<std::size_t>(c)] = row[0] * xyz[0] + row[1] * xyz[1] + row[2] * xyz[2];
  }
}

inline void IridescenceFresnel(double cosTheta1, double thicknessNm, double filmIor,
                               const std::array<double, 3> &baseF0, std::array<double, 3> &out) {
  const double outsideIor = kOutsideIor;
  const double sinTheta2Sq = (outsideIor / filmIor) * (outsideIor / filmIor) * (1.0 - cosTheta1 * cosTheta1);
  const double cosTheta2Sq = 1.0 - sinTheta2Sq;
  if (cosTheta2Sq < 0.0) {
    out = {1.0, 1.0, 1.0};
    return;
  }
  const double cosTheta2 = std::sqrt(cosTheta2Sq);

  const double r12 = IridescenceSchlick(IorToFresnel0(filmIor, outsideIor), cosTheta1);
  const double t121 = 1.0 - r12;
  const double phi12 = filmIor < outsideIor ? kBrdfPi : 0.0;
  const double phi21 = kBrdfPi - phi12;

  std::array<double, 3> r23{};
  std::array<double, 3> phi{};
  for (int c = 0; c < 3; ++c) {
    const std::size_t i = static_cast<std::size_t>(c);
    const double baseIor = Fresnel0ToIor(baseF0[i] + 0.0001);
    r23[i] = IridescenceSchlick(IorToFresnel0(baseIor, filmIor), cosTheta2);
    phi[i] = phi21 + (baseIor < filmIor ? kBrdfPi : 0.0);
  }

  const double opd = 2.0 * filmIor * thicknessNm * cosTheta2;

  std::array<double, 3> cm{};
  std::array<double, 3> r123root{};
  for (int c = 0; c < 3; ++c) {
    const std::size_t i = static_cast<std::size_t>(c);
    const double r123 = std::fmin(std::fmax(r12 * r23[i], 1e-5), 0.9999);
    const double rs = t121 * t121 * r23[i] / (1.0 - r123);
    out[i] = r12 + rs;
    cm[i] = rs - t121;
    r123root[i] = std::sqrt(r123);
  }
  for (int m = 1; m <= 2; ++m) {
    const std::array<double, 3> shift{m * phi[0], m * phi[1], m * phi[2]};
    std::array<double, 3> sm{};
    IridescenceSensitivity(static_cast<double>(m) * opd, shift, sm);
    for (int c = 0; c < 3; ++c) {
      const std::size_t i = static_cast<std::size_t>(c);
      cm[i] *= r123root[i];
      out[i] += cm[i] * 2.0 * sm[i];
    }
  }

  for (double &v : out) { v = std::fmin(std::fmax(v, 0.0), 1.0); }
}

[[nodiscard]] inline std::string IridescenceLobeMsl(void) {
  char constants[1024];
  std::snprintf(constants, sizeof constants,
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
                kOutsideIor, kFresnelInverseCeiling,
                kSensitivityVal[0], kSensitivityVal[1], kSensitivityVal[2],
                kSensitivityPos[0], kSensitivityPos[1], kSensitivityPos[2],
                kSensitivityVar[0], kSensitivityVar[1], kSensitivityVar[2],
                kSensitivityValX2, kSensitivityPosX2, kSensitivityVarX2, kSensitivityNorm,

                kXyzToRec709[0][0], kXyzToRec709[1][0], kXyzToRec709[2][0],
                kXyzToRec709[0][1], kXyzToRec709[1][1], kXyzToRec709[2][1],
                kXyzToRec709[0][2], kXyzToRec709[1][2], kXyzToRec709[2][2]);
  return std::string(constants) + R"(
static inline float iridescenceIorToF0(float transmitted, float incident) {
  float d = (transmitted - incident) / (transmitted + incident);
  return d * d;
}

static inline float3 iridescenceF0ToIor(float3 f0) {
  float3 s = sqrt(clamp(f0, 0.0, kIriF0Ceiling));
  return (1.0 + s) / (1.0 - s);
}

static inline float iridescenceSchlick(float f0, float cosTheta) {
  float m = 1.0 - cosTheta;
  float m2 = m * m;
  return f0 + (1.0 - f0) * m2 * m2 * m;
}

static inline float3 iridescenceSchlick3(float3 f0, float cosTheta) {
  float m = 1.0 - cosTheta;
  float m2 = m * m;
  return f0 + (1.0 - f0) * m2 * m2 * m;
}

static inline float3 iridescenceSensitivity(float opdNm, float3 shift) {
  float phase = 2.0 * kPi * opdNm * 1.0e-9;
  float3 xyz = kIriVal * sqrt(2.0 * kPi * kIriVar) * cos(kIriPos * phase + shift) *
               exp(-phase * phase * kIriVar);
  xyz.x += kIriValX2 * sqrt(2.0 * kPi * kIriVarX2) * cos(kIriPosX2 * phase + shift.x) *
           exp(-kIriVarX2 * phase * phase);
  return kIriXyzToRgb * (xyz / kIriNorm);
}

static inline float3 iridescenceFresnel(float cosTheta1, float thicknessNm, float filmIor,
                                        float3 baseF0) {
  float sinTheta2Sq = (kIriOutsideIor / filmIor) * (kIriOutsideIor / filmIor) *
                      (1.0 - cosTheta1 * cosTheta1);
  float cosTheta2Sq = 1.0 - sinTheta2Sq;
  if (cosTheta2Sq < 0.0) { return float3(1.0); }
  float cosTheta2 = sqrt(cosTheta2Sq);

  float r12 = iridescenceSchlick(iridescenceIorToF0(filmIor, kIriOutsideIor), cosTheta1);
  float t121 = 1.0 - r12;
  float phi12 = filmIor < kIriOutsideIor ? kPi : 0.0;
  float phi21 = kPi - phi12;

  float3 baseIor = iridescenceF0ToIor(baseF0 + 0.0001);
  float3 r1 = (baseIor - filmIor) / (baseIor + filmIor);
  float3 r23 = iridescenceSchlick3(r1 * r1, cosTheta2);
  float3 phi = float3(phi21) + select(float3(0.0), float3(kPi), baseIor < filmIor);

  float opd = 2.0 * filmIor * thicknessNm * cosTheta2;

  float3 r123 = clamp(r12 * r23, 1.0e-5, 0.9999);
  float3 r123root = sqrt(r123);
  float3 rs = t121 * t121 * r23 / (1.0 - r123);
  float3 f = r12 + rs;
  float3 cm = rs - t121;
  for (int m = 1; m <= 2; ++m) {
    cm *= r123root;
    f += cm * 2.0 * iridescenceSensitivity(float(m) * opd, float(m) * phi);
  }
  return clamp(f, 0.0, 1.0);
}
)";
}

}

#endif
