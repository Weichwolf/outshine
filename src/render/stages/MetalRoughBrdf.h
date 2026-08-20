#ifndef METALROUGHBRDF_H
#define METALROUGHBRDF_H

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace outshine::Render {

constexpr double kDielectricF0 = 0.04;
constexpr double kBrdfPi = 3.141592653589793;

[[nodiscard]] inline double BrdfDistribution(double nh, double a2) {
  const double denominator = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (kBrdfPi * denominator * denominator);
}

[[nodiscard]] inline double BrdfVisibility(double nl, double nv, double a2) {
  return 0.5 / (nl * std::sqrt(nv * nv * (1.0 - a2) + a2) +
                nv * std::sqrt(nl * nl * (1.0 - a2) + a2));
}

[[nodiscard]] inline std::array<double, 3> BrdfFresnel(const std::array<double, 3> &f0, double f90,
                                                      double vh) {
  const double grazing = 1.0 - vh;
  const double squared = grazing * grazing;
  const double weight = squared * squared * grazing;
  return {f0[0] + (f90 - f0[0]) * weight, f0[1] + (f90 - f0[1]) * weight,
          f0[2] + (f90 - f0[2]) * weight};
}

[[nodiscard]] inline double ToksvigA2(double a2, double l) {
  const double d = l * a2 + 2.0 * (1.0 - a2) * (1.0 - l);
  const double n = 2.0 * l * (1.0 - a2);
  const double denominator = n + 2.0 * d;
  return denominator > 0.0 ? 2.0 * d / denominator : 1.0;
}

[[nodiscard]] inline double RoughenedBy(double roughness, double meanResultantLength) {

  if (!(meanResultantLength < 1.0)) { return roughness; }
  const double alpha = roughness * roughness;
  return std::sqrt(std::sqrt(ToksvigA2(alpha * alpha, meanResultantLength)));
}

[[nodiscard]] inline double BrdfAnisotropicDistribution(double nh, double th, double bh, double at,
                                                        double ab) {
  const double a2 = at * ab;
  const double fx = ab * th, fy = at * bh, fz = a2 * nh;
  const double dot = fx * fx + fy * fy + fz * fz;
  if (!(dot > 0.0)) { return 0.0; }
  const double w2 = a2 / dot;
  return a2 * w2 * w2 / kBrdfPi;
}

[[nodiscard]] inline double BrdfAnisotropicVisibility(double nl, double nv, double tv, double bv,
                                                      double tl, double bl, double at, double ab) {
  const double alongV = nl * std::sqrt(at * tv * at * tv + ab * bv * ab * bv + nv * nv);
  const double alongL = nv * std::sqrt(at * tl * at * tl + ab * bl * ab * bl + nl * nl);
  const double sum = alongV + alongL;
  if (!(sum > 0.0)) { return 0.0; }
  const double v = 0.5 / sum;
  return v > 1.0 ? 1.0 : v;
}

struct BrdfTerms {
  std::array<double, 3> Diffuse{0.0, 0.0, 0.0};
  std::array<double, 3> Specular{0.0, 0.0, 0.0};
};

struct BrdfGeometry {
  double Nl = 0;
  double Nv = 0;
  double Nh = 0;
  double Vh = 0;
};

[[nodiscard]] inline double BrdfLobe(double a2, const BrdfGeometry &at) {
  return a2 > 0.0 ? BrdfDistribution(at.Nh, a2) * BrdfVisibility(at.Nl, at.Nv, a2) : 0.0;
}

[[nodiscard]] inline BrdfTerms BrdfCombine(const std::array<double, 3> &diffuseColour,
                                           const std::array<double, 3> &fresnel, double lobe) {
  BrdfTerms terms;
  for (size_t channel = 0; channel < 3; ++channel) {
    terms.Diffuse[channel] = (1.0 - fresnel[channel]) * diffuseColour[channel] * (1.0 / kBrdfPi);
    terms.Specular[channel] = fresnel[channel] * lobe;
  }
  return terms;
}

[[nodiscard]] inline BrdfTerms BrdfRgbMix(const std::array<double, 3> &diffuseColour,
                                          const std::array<double, 3> &fresnel, double lobe) {
  const double most = std::fmax(std::fmax(fresnel[0], fresnel[1]), fresnel[2]);
  BrdfTerms terms;
  for (size_t channel = 0; channel < 3; ++channel) {
    terms.Diffuse[channel] = (1.0 - most) * diffuseColour[channel] * (1.0 / kBrdfPi);
    terms.Specular[channel] = fresnel[channel] * lobe;
  }
  return terms;
}

[[nodiscard]] inline BrdfTerms MetalRoughBrdf(const std::array<double, 3> &diffuseColour,
                                              const std::array<double, 3> &f0, double f90, double a2,
                                              const BrdfGeometry &at) {
  return BrdfCombine(diffuseColour, BrdfFresnel(f0, f90, at.Vh), BrdfLobe(a2, at));
}

[[nodiscard]] inline std::string MetalRoughBrdfMsl(void) {
  char constants[256];
  std::snprintf(constants, sizeof constants,
                "constant float kPi = %.17g;\n", kBrdfPi);
  return std::string(constants) + R"(
struct Brdf { float3 diffuse; float3 specular; };

static inline float max3(float3 v) { return max(max(v.x, v.y), v.z); }

static inline float brdfAnisotropicDistribution(float nh, float th, float bh, float at, float ab) {
  float a2 = at * ab;
  float3 f = float3(ab * th, at * bh, a2 * nh);
  float d = dot(f, f);
  if (!(d > 0.0)) { return 0.0; }
  float w2 = a2 / d;
  return a2 * w2 * w2 / kPi;
}

static inline float brdfAnisotropicVisibility(float nl, float nv, float tv, float bv, float tl,
                                              float bl, float at, float ab) {
  float alongV = nl * length(float3(at * tv, ab * bv, nv));
  float alongL = nv * length(float3(at * tl, ab * bl, nl));
  float s = alongV + alongL;
  if (!(s > 0.0)) { return 0.0; }
  return clamp(0.5 / s, 0.0, 1.0);
}

static inline float brdfDistribution(float nh, float a2) {
  float denominator = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (kPi * denominator * denominator);
}

static inline float brdfVisibility(float nl, float nv, float a2) {
  return 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
}

static inline float toksvigA2(float a2, float l) {
  float d = l * a2 + 2.0 * (1.0 - a2) * (1.0 - l);
  float n = 2.0 * l * (1.0 - a2);
  float denominator = n + 2.0 * d;
  return denominator > 0.0 ? 2.0 * d / denominator : 1.0;
}

static inline float roughenedBy(float roughness, float meanResultantLength) {
  if (!(meanResultantLength < 1.0)) { return roughness; }
  float alpha = roughness * roughness;
  return sqrt(sqrt(toksvigA2(alpha * alpha, meanResultantLength)));
}

static inline float3 brdfFresnel(float3 f0, float f90, float vh) {
  float grazing = 1.0 - vh;
  float squared = grazing * grazing;
  return f0 + (float3(f90) - f0) * (squared * squared * grazing);
}

static inline float brdfLobe(float a2, float nl, float nv, float nh) {
  if (!(a2 > 0.0)) { return 0.0; }
  return brdfDistribution(nh, a2) * brdfVisibility(nl, nv, a2);
}

static inline Brdf brdfCombine(float3 diffuseColour, float3 fresnel, float lobe) {
  Brdf terms;
  terms.diffuse = (float3(1.0) - fresnel) * diffuseColour * (1.0 / kPi);
  terms.specular = fresnel * lobe;
  return terms;
}

static inline Brdf brdfRgbMix(float3 diffuseColour, float3 fresnel, float lobe) {
  Brdf terms;
  terms.diffuse = (1.0 - max3(fresnel)) * diffuseColour * (1.0 / kPi);
  terms.specular = fresnel * lobe;
  return terms;
}

static inline Brdf metalRoughBrdf(float3 diffuseColour, float3 f0, float f90, float a2,
                                  float nl, float nv, float nh, float vh) {
  return brdfCombine(diffuseColour, brdfFresnel(f0, f90, vh), brdfLobe(a2, nl, nv, nh));
}
)";
}

}
#endif
