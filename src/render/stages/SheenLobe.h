/* `KHR_materials_sheen`: THE "CHARLIE" LOBE, STATED ONCE IN C++ AND EMITTED FROM IT (board:1385).
 *
 * It sits beside `MetalRoughBrdf.h` and on the same terms: the MSL half is not written by hand, it is
 * produced from these functions, so a term added to one cannot be missing from the other. What the
 * extension specifies is quoted rather than recalled -- the distribution, the visibility and the
 * albedo-scaling that keeps the layered surface from gaining energy.
 *
 * THE DIRECTIONAL ALBEDO IS INTEGRATED FROM THIS LOBE AND NOT FETCHED, and that is the one decision
 * here that is ours rather than the format's. `KHR_materials_sheen` points at a 16x16 table in a
 * third-party document -- *res/Sheen_E.exr* of the Enterprise PBR Shading Model -- and offers no
 * analytic fit. A copied table is a number that can drift away from the lobe it belongs to the moment
 * the lobe is touched; **an integral over our own lobe cannot**. It is evaluated once, off the frame
 * path, and the shader's table is generated from the same integral at emit time.
 *
 * THE INTEGRAL IS THE EXTENSION'S OWN: E(mu_v; r) = the hemispherical integral of the lobe times the
 * light cosine, which is what "how much of the arriving light this layer sends somewhere" means. */
#ifndef SHEENLOBE_H
#define SHEENLOBE_H

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace outshine::Render {

inline constexpr double kSheenPi = 3.14159265358979323846;

/* The specification's own words for the distribution, and the ImageWorks form behind them:
 *
 *   alpha_g = sheenRoughness * sheenRoughness
 *   inv_r   = 1 / alpha_g
 *   sin2h   = 1 - NdotH * NdotH
 *   D       = (2 + inv_r) * pow(sin2h, inv_r * 0.5) / (2 * PI)
 *
 * A ZERO ROUGHNESS HAS NO LOBE AND IS NOT A DIVISION TO GUARD. `alpha_g` at zero makes `inv_r`
 * unbounded and the distribution a Dirac; the same argument the metal-rough half already makes for
 * `alpha = 0` applies, so the answer is no sheen rather than an invented floor. */
[[nodiscard]] inline double SheenDistribution(double nh, double roughness) {
  const double alpha = roughness * roughness;
  if (!(alpha > 0.0)) { return 0.0; }
  const double inverse = 1.0 / alpha;
  const double sin2h = 1.0 - nh * nh;
  if (!(sin2h > 0.0)) { return 0.0; }
  return (2.0 + inverse) * std::pow(sin2h, inverse * 0.5) / (2.0 * kSheenPi);
}

/* The "Charlie" visibility, quoted from the extension including its five fitted coefficients and the
 * two ends they are mixed between. The mix parameter is `(1 - alpha_g)^2` and not `alpha_g`, which is
 * the shape a transcription most easily gets wrong. */
[[nodiscard]] inline double SheenLambdaFit(double x, double alpha) {
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

[[nodiscard]] inline double SheenLambda(double cosTheta, double alpha) {
  return std::fabs(cosTheta) < 0.5
             ? std::exp(SheenLambdaFit(cosTheta, alpha))
             : std::exp(2.0 * SheenLambdaFit(0.5, alpha) - SheenLambdaFit(1.0 - cosTheta, alpha));
}

[[nodiscard]] inline double SheenVisibility(double nl, double nv, double roughness) {
  const double alpha = roughness * roughness;
  if (!(alpha > 0.0) || !(nl > 0.0) || !(nv > 0.0)) { return 0.0; }
  return 1.0 / ((1.0 + SheenLambda(nv, alpha) + SheenLambda(nl, alpha)) * (4.0 * nv * nl));
}

/* HOW MANY STEPS THE INTEGRAL TAKES, and the number is [SET] rather than derived: the table it fills
 * is 16 x 16 because that is the resolution the extension's own reference table uses, and the inner
 * integration is finer than the table it feeds so that the table's error is the table's spacing and
 * not the quadrature's. */
inline constexpr int kSheenAlbedoSteps = 16;
inline constexpr int kSheenAlbedoQuadrature = 64;

/* E(mu_v; r): the fraction of arriving light this layer sends anywhere, integrated over the
 * hemisphere. `phi` runs the azimuth and `mu_l` the light cosine; the half vector is rebuilt from
 * the two directions the way a shading point would. */
[[nodiscard]] inline double SheenDirectionalAlbedo(double nv, double roughness) {
  if (!(roughness > 0.0) || !(nv > 0.0)) { return 0.0; }
  const double sinV = std::sqrt(1.0 - nv * nv);
  double total = 0.0;
  for (int azimuth = 0; azimuth < kSheenAlbedoQuadrature; ++azimuth) {
    const double phi = (azimuth + 0.5) * 2.0 * kSheenPi / kSheenAlbedoQuadrature;
    for (int elevation = 0; elevation < kSheenAlbedoQuadrature; ++elevation) {
      const double nl = (elevation + 0.5) / kSheenAlbedoQuadrature;
      const double sinL = std::sqrt(1.0 - nl * nl);
      /* The half vector of a view at `nv` and a light at `nl` separated by `phi` in azimuth. */
      const double hx = sinV + sinL * std::cos(phi);
      const double hy = sinL * std::sin(phi);
      const double hz = nv + nl;
      const double length = std::sqrt(hx * hx + hy * hy + hz * hz);
      if (!(length > 0.0)) { continue; }
      const double nh = hz / length;
      total += SheenDistribution(nh, roughness) * SheenVisibility(nl, nv, roughness) * nl * nl;
    }
  }
  const double dPhi = 2.0 * kSheenPi / kSheenAlbedoQuadrature;
  const double dMu = 1.0 / kSheenAlbedoQuadrature;
  return total * dPhi * dMu;
}

/* THE DEVICE HALF, GENERATED FROM THE FUNCTIONS ABOVE. The table is written into the shader text as
 * the integral's own output, so the two halves cannot state different numbers -- which is the whole
 * reason the albedo is derived here rather than copied from a document. */
[[nodiscard]] inline std::string SheenLobeMsl(void) {
  std::string table;
  table.reserve(kSheenAlbedoSteps * kSheenAlbedoSteps * 12);
  for (int r = 0; r < kSheenAlbedoSteps; ++r) {
    const double roughness = (r + 0.5) / kSheenAlbedoSteps;
    for (int v = 0; v < kSheenAlbedoSteps; ++v) {
      const double nv = (v + 0.5) / kSheenAlbedoSteps;
      char cell[32];
      std::snprintf(cell, sizeof cell, "%s%.6ff", table.empty() ? "" : ", ",
                    SheenDirectionalAlbedo(nv, roughness));
      table += cell;
    }
  }
  char head[192];
  std::snprintf(head, sizeof head, "constant int kSheenSteps = %d;\n", kSheenAlbedoSteps);
  return std::string(head) + "constant float kSheenAlbedo[] = { " + table + " };\n" + R"(
static inline float sheenDistribution(float nh, float roughness) {
  float alpha = roughness * roughness;
  if (!(alpha > 0.0)) { return 0.0; }
  float inverse = 1.0 / alpha;
  float sin2h = 1.0 - nh * nh;
  if (!(sin2h > 0.0)) { return 0.0; }
  return (2.0 + inverse) * pow(sin2h, inverse * 0.5) / (2.0 * kPi);
}

static inline float sheenLambdaFit(float x, float alpha) {
  float smooth = (1.0 - alpha) * (1.0 - alpha);
  float a = mix(21.5473, 25.3245, smooth);
  float b = mix(3.82987, 3.32435, smooth);
  float c = mix(0.19823, 0.16801, smooth);
  float d = mix(-1.97760, -1.27393, smooth);
  float e = mix(-4.32054, -4.85967, smooth);
  return a / (1.0 + b * pow(x, c)) + d * x + e;
}

static inline float sheenLambda(float cosTheta, float alpha) {
  return fabs(cosTheta) < 0.5 ? exp(sheenLambdaFit(cosTheta, alpha))
                              : exp(2.0 * sheenLambdaFit(0.5, alpha) -
                                    sheenLambdaFit(1.0 - cosTheta, alpha));
}

static inline float sheenVisibility(float nl, float nv, float roughness) {
  float alpha = roughness * roughness;
  if (!(alpha > 0.0) || !(nl > 0.0) || !(nv > 0.0)) { return 0.0; }
  return 1.0 / ((1.0 + sheenLambda(nv, alpha) + sheenLambda(nl, alpha)) * (4.0 * nv * nl));
}

/* THE TABLE IS READ AT THE CELL CENTRES IT WAS BUILT AT, with no interpolation: sixteen steps over a
 * quantity that varies slowly, and a lerp here would be a second statement about a curve the C++
 * half already sampled. */
static inline float sheenAlbedo(float nv, float roughness) {
  int r = clamp(int(roughness * float(kSheenSteps)), 0, kSheenSteps - 1);
  int v = clamp(int(nv * float(kSheenSteps)), 0, kSheenSteps - 1);
  return kSheenAlbedo[r * kSheenSteps + v];
}

/* WHAT THE BASE LAYER KEEPS, so the two together send out no more than arrived: the extension's own
 * simplified form, which uses the view term alone. */
static inline float sheenAlbedoScaling(float3 sheenColour, float nv, float roughness) {
  float strongest = max(max(sheenColour.x, sheenColour.y), sheenColour.z);
  return 1.0 - strongest * sheenAlbedo(nv, roughness);
}
)";
}

} // namespace outshine::Render
#endif
