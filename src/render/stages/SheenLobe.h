#ifndef OUTSHINE_RENDER_STAGES_SHEENLOBE_H
#define OUTSHINE_RENDER_STAGES_SHEENLOBE_H

#include <numbers>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "Lobe.h"
#include "ShaderFile.h"

namespace outshine::Render {

constexpr size_t kPreludeBytes = 192;

inline constexpr double kSheenPi = std::numbers::pi;

[[nodiscard]] inline double SheenDistribution(Slant at) {
  const double nh = at.Cosine;
  const double roughness = at.Roughness;
  const double alpha = roughness * roughness;
  if (!(alpha > 0.0)) { return 0.0; }
  const double inverse = 1.0 / alpha;
  const double sin2h = 1.0 - nh * nh;
  if (!(sin2h > 0.0)) { return 0.0; }
  return (2.0 + inverse) * std::pow(sin2h, inverse * 0.5) / (2.0 * kSheenPi);
}

[[nodiscard]] inline double SheenLambdaFit(Slant at) {
  const double x = at.Cosine;
  const double alpha = at.Roughness;
  const double smooth = (1.0 - alpha) * (1.0 - alpha);
  const auto mix = [smooth](double rough, double polished) {
    return rough + (polished - rough) * smooth;
  };
  const double a = mix(21.5473, 25.3245);
  const double b = mix(3.82987, 3.32435);
  const double c = mix(0.19823, 0.16801);
  const double d = mix(-1.97760, -1.27393);
  const double e = mix(-4.32054, -4.85967);
  return a / (1.0 + b * std::pow(x, c)) + d * x + e;
}

[[nodiscard]] inline double SheenLambda(Slant at) {
  const double cosTheta = at.Cosine;
  const double alpha = at.Roughness;
  return std::fabs(cosTheta) < 0.5
             ? std::exp(SheenLambdaFit({.Cosine = cosTheta, .Roughness = alpha}))
             : std::exp(2.0 * SheenLambdaFit({.Cosine = 0.5, .Roughness = alpha}) -
                        SheenLambdaFit({.Cosine = 1.0 - cosTheta, .Roughness = alpha}));
}

[[nodiscard]] inline double SheenVisibility(Grazing over, double roughness) {
  const double nl = over.NoL;
  const double nv = over.NoV;
  const double alpha = roughness * roughness;
  if (!(alpha > 0.0) || !(nl > 0.0) || !(nv > 0.0)) { return 0.0; }
  return 1.0 / ((1.0 + SheenLambda({.Cosine = nv, .Roughness = alpha}) +
                 SheenLambda({.Cosine = nl, .Roughness = alpha})) *
                (4.0 * nv * nl));
}

inline constexpr int kSheenAlbedoSteps = 16;
inline constexpr int kSheenAlbedoQuadrature = 64;

[[nodiscard]] inline double SheenDirectionalAlbedo(Slant at) {
  const double nv = at.Cosine;
  const double roughness = at.Roughness;
  if (!(roughness > 0.0) || !(nv > 0.0)) { return 0.0; }
  const double sinV = std::sqrt(1.0 - nv * nv);
  double total = 0.0;
  for (int azimuth = 0; azimuth < kSheenAlbedoQuadrature; ++azimuth) {
    const double phi = (azimuth + 0.5) * 2.0 * kSheenPi / kSheenAlbedoQuadrature;
    for (int elevation = 0; elevation < kSheenAlbedoQuadrature; ++elevation) {
      const double nl = (elevation + 0.5) / kSheenAlbedoQuadrature;
      const double sinL = std::sqrt(1.0 - nl * nl);

      const double hx = sinV + sinL * std::cos(phi);
      const double hy = sinL * std::sin(phi);
      const double hz = nv + nl;
      const double length = std::sqrt(hx * hx + hy * hy + hz * hz);
      if (!(length > 0.0)) { continue; }
      const double nh = hz / length;
      total += SheenDistribution({.Cosine = nh, .Roughness = roughness}) *
               SheenVisibility({.NoL = nl, .NoV = nv}, roughness) * nl * nl;
    }
  }
  const double dPhi = 2.0 * kSheenPi / kSheenAlbedoQuadrature;
  const double dMu = 1.0 / kSheenAlbedoQuadrature;
  return total * dPhi * dMu;
}

inline ShaderText &SheenLobe(ShaderText &into) {
  std::string table;
  table.reserve(size_t{kSheenAlbedoSteps} * kSheenAlbedoSteps * 12);
  for (int r = 0; r < kSheenAlbedoSteps; ++r) {
    const double roughness = (r + 0.5) / kSheenAlbedoSteps;
    for (int v = 0; v < kSheenAlbedoSteps; ++v) {
      const double nv = (v + 0.5) / kSheenAlbedoSteps;
      std::array<char, 32> cell{};
      std::snprintf(cell.data(),
                    cell.size(),
                    "%s%.6ff",
                    table.empty() ? "" : ", ",
                    SheenDirectionalAlbedo({.Cosine = nv, .Roughness = roughness}));
      table += cell.data();
    }
  }
  std::array<char, kPreludeBytes> head{};
  std::snprintf(head.data(), head.size(), "constant int kSheenSteps = %d;\n", kSheenAlbedoSteps);
  return into.Adds(head.data())
      .Adds("constant float kSheenAlbedo[] = { ")
      .Adds(table)
      .Adds(" };\n")
      .Reads("src/render/shaders/sheenLobe.msl");
}

[[nodiscard]] inline std::string SheenLobeMsl(std::string &error) {
  ShaderText source;
  return SheenLobe(source).Take(error);
}

[[nodiscard]] inline std::string SheenLobeMsl() {
  std::string ignored;
  return SheenLobeMsl(ignored);
}

} // namespace outshine::Render
#endif
