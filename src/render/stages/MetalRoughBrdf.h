/* glTF 2.0's OWN metal-rough BRDF, term for term out of the specification's Appendix B, in C++ and
 * in MSL. The Khronos corpus states its criteria in this model, so an engine that shaded a house
 * approximation would be measuring the approximation.
 *
 * TWO HALVES OF ONE FORMULA: a shading term has to run on the device and has to be integrable on the
 * host, and no language spans both. The C++ half is the definition and the MSL half is its
 * transliteration. Three rules keep them from drifting: (a) no numeric constant is typed in the MSL
 * at all -- they are emitted from the C++ ones by `MetalRoughBrdfMsl()`; (b) the MSL declares no term
 * the C++ half does not, so a new term has to be written twice on purpose rather than once by
 * accident; (c) the ARRANGEMENT of the terms is measured -- a shader test
 * runs this text on the device over a sample set and compares it against these functions, and carries
 * a mutant of its own to show the comparison can see a scaled term.
 *
 * `alpha = 0` MEANS NO SPECULAR AT ALL, and that is the physics rather than a guard against a
 * division. A perfectly smooth surface has a Dirac lobe and a light with no area is a Dirac source;
 * the probability that the two coincide at a shading point is zero, so the correct answer against a
 * punctual light is zero specular everywhere -- which is also what Cycles returns for a roughness-0
 * GGX under a delta light, so the oracle can confirm it. Clamping the roughness to an invented floor
 * would manufacture a highlight out of a number nobody derived. Against an AREA of directions the
 * same arm reads as a loss instead: a mirror's directional albedo is F and this returns 0, which is
 * what this layer's own furnace sweep prints at roughness 0 and does not refuse. */
#ifndef METALROUGHBRDF_H
#define METALROUGHBRDF_H

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace outshine::Render {

/* glTF's dielectric normal-incidence reflectance at its default IOR of 1.5, which is what
 * `core/Material.h`'s `DielectricF0` returns for a material declaring neither `KHR_materials_ior` nor
 * `KHR_materials_specular`. IT IS NO LONGER EMITTED INTO THE SHADER (board:1205): the row carries the
 * computed F0 now, so a constant in the shader text would be a second answer to the same question and
 * the fragment reads the one it was handed. What is left here is a NAME for the default, used by the
 * lobe's own tests to pick a representative dielectric. */
constexpr double kDielectricF0 = 0.04;
constexpr double kBrdfPi = 3.141592653589793;

/* Trowbridge-Reitz (GGX). `a2` is alpha squared and alpha is roughness squared, which is glTF's own
 * remapping and not a house one. */
[[nodiscard]] inline double BrdfDistribution(double nh, double a2) {
  const double denominator = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (kBrdfPi * denominator * denominator);
}

/* Height-correlated Smith, carrying the `1 / (4 |N.L| |N.V|)` of the microfacet denominator inside
 * it -- so this is a visibility and not a masking term, and multiplying by 1/(4 nl nv) again would
 * be the commonest way to halve a highlight twice. */
[[nodiscard]] inline double BrdfVisibility(double nl, double nv, double a2) {
  return 0.5 / (nl * std::sqrt(nv * nv * (1.0 - a2) + a2) +
                nv * std::sqrt(nl * nl * (1.0 - a2) + a2));
}

/* Schlick, on the HALF-VECTOR angle. `vh` and not `nv`: the Fresnel of a microfacet model is the
 * one at the facet that actually reflects V into L. The fifth power is spelled as a product and not
 * as `pow(x, 5)` so that the two halves perform the SAME four multiplications: a device `pow` is
 * `exp2(5 * log2(x))` under a relaxed-precision compiler and its error is thousands of ulps, which
 * would put a transcendental's accuracy inside the tie that compares the halves. */
[[nodiscard]] inline std::array<double, 3> BrdfFresnel(const std::array<double, 3> &f0, double vh) {
  const double grazing = 1.0 - vh;
  const double squared = grazing * grazing;
  const double weight = squared * squared * grazing;
  return {f0[0] + (1.0 - f0[0]) * weight, f0[1] + (1.0 - f0[1]) * weight,
          f0[2] + (1.0 - f0[2]) * weight};
}

/* THE PERTURBATION A MIP CHAIN AVERAGED AWAY, RETURNED AS ROUGHNESS (board:1130). `l` is the mean
 * resultant length of the normals a texel stands for -- 1 where nothing was averaged, shorter as they
 * diverge -- and it arrives in the normal texture's alpha, which glTF gives no meaning.
 *
 * TOKSVIG 2005 states it for Blinn-Phong: a lobe of exponent `s` under normals of that spread behaves
 * as `s' = s*l / (l + s*(1 - l))`. The bridge to GGX is `s = 2/a2 - 2` (Real-Time Rendering 4e, 9.8.1).
 * Substituting and clearing `a2` from the denominators:
 *
 *     N = 2l(1 - a2)                D = l*a2 + 2(1 - a2)(1 - l)
 *     s' = N/D                      a2' = 2/(s' + 2) = 2D/(N + 2D)
 *
 * WHICH DIVIDES ONCE WHERE THE DIRECT FORM DIVIDES TWICE, and that is the whole reason for the
 * rearrangement rather than a preference: `s` is unbounded as `a2` falls to zero, so the direct form
 * needs a clamp exactly where this engine refuses to invent one. THE LIMITS ARE EXACT AND NOT
 * APPROACHED -- at `l = 1` this is `a2`; at `a2 = 0` with `l = 1` it is 0, so a mirror stays a mirror
 * and the `alpha = 0` arm above still means what it says; as `l` falls to 0 it rises to 1. IN EXACT
 * ARITHMETIC, and `RoughenedBy` below says why that qualifier is load-bearing rather than pedantic.
 *
 * ITS ONE DEGENERATE POINT IS NAMED RATHER THAN CLAMPED: `N + 2D = 2l + 4(1 - a2)(1 - l)` vanishes only
 * at `l = 0` with `a2 = 1`, four exactly opposing normals on a surface that is already fully rough, and
 * 1 is both the limit from every direction and the value that surface already had. */
[[nodiscard]] inline double ToksvigA2(double a2, double l) {
  const double d = l * a2 + 2.0 * (1.0 - a2) * (1.0 - l);
  const double n = 2.0 * l * (1.0 - a2);
  const double denominator = n + 2.0 * d;
  return denominator > 0.0 ? 2.0 * d / denominator : 1.0;
}

/* The same correction in glTF's own currency, so a caller never has to know that `alpha` is roughness
 * squared and `a2` is that squared again. */
[[nodiscard]] inline double RoughenedBy(double roughness, double meanResultantLength) {
  /* THE IDENTITY IS TAKEN AS AN IDENTITY AND NOT COMPUTED (board:1130). `ToksvigA2(a2, 1)` IS `a2` in
   * exact arithmetic, but reaching it through `r*r`, `alpha*alpha` and two roots does not return `r`
   * in binary32 -- measured, an unfiltered picture moved in its fourth decimal for the code merely
   * being present, which is a correction leaking into the case it was defined to leave alone. */
  if (!(meanResultantLength < 1.0)) { return roughness; }
  const double alpha = roughness * roughness;
  return std::sqrt(std::sqrt(ToksvigA2(alpha * alpha, meanResultantLength)));
}

/* `KHR_materials_anisotropy`: THE SAME GGX WITH TWO ROUGHNESSES INSTEAD OF ONE, quoted from the
 * extension. `at` is the roughness along the anisotropy direction and `ab` across it, and the
 * parametrisation is the extension's own: *at = mix(alphaRoughness, 1.0, anisotropy * anisotropy)*,
 * `ab = alphaRoughness`. **A strength of zero makes the two equal and the lobe round again**, which
 * is why no branch guards it and why a material declaring none reaches the same arithmetic it had.
 *
 * IT NEEDS A TANGENT FRAME AND THE FORMAT SAYS SO: *a mesh primitive using an anisotropy material MUST
 * have a defined tangent space*. Where none is carried the direction has no meaning, and the caller
 * hands a zero vector rather than inventing one. */
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

/* The two halves the specification splits the surface into, kept apart in the return because the
 * furnace integrates them separately: the microfacet lobe is what `DirectionalLight` is named for,
 * and the diffuse term is coupled to it only through `1 - F`. */
struct BrdfTerms {
  std::array<double, 3> Diffuse{0.0, 0.0, 0.0};
  std::array<double, 3> Specular{0.0, 0.0, 0.0};
};

/* The cosines a shading point hands the model, so the caller states its geometry once instead of
 * four times in a row (`I.23`). Every one of them is a clamped dot product, dimensionless. */
struct BrdfGeometry {
  double Nl = 0;
  double Nv = 0;
  double Nh = 0;
  double Vh = 0;
};

[[nodiscard]] inline BrdfTerms MetalRoughBrdf(const std::array<double, 3> &diffuseColour,
                                              const std::array<double, 3> &f0, double a2,
                                              const BrdfGeometry &at) {
  const std::array<double, 3> fresnel = BrdfFresnel(f0, at.Vh);
  BrdfTerms terms;
  const double lobe = a2 > 0.0 ? BrdfDistribution(at.Nh, a2) * BrdfVisibility(at.Nl, at.Nv, a2) : 0.0;
  for (size_t channel = 0; channel < 3; ++channel) {
    terms.Diffuse[channel] = (1.0 - fresnel[channel]) * diffuseColour[channel] * (1.0 / kBrdfPi);
    terms.Specular[channel] = fresnel[channel] * lobe;
  }
  return terms;
}

/* The device half. A textual splice, like the emitters beside it -- never compiled alone. */
[[nodiscard]] inline std::string MetalRoughBrdfMsl(void) {
  char constants[256];
  std::snprintf(constants, sizeof constants,
                "constant float kPi = %.17g;\n", kBrdfPi);
  return std::string(constants) + R"(
struct Brdf { float3 diffuse; float3 specular; };

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

static inline float3 brdfFresnel(float3 f0, float vh) {
  float grazing = 1.0 - vh;
  float squared = grazing * grazing;
  return f0 + (float3(1.0) - f0) * (squared * squared * grazing);
}

static inline Brdf metalRoughBrdf(float3 diffuseColour, float3 f0, float a2,
                                  float nl, float nv, float nh, float vh) {
  float3 fresnel = brdfFresnel(f0, vh);
  Brdf terms;
  float lobe = 0.0;
  if (a2 > 0.0) { lobe = brdfDistribution(nh, a2) * brdfVisibility(nl, nv, a2); }
  terms.diffuse = (float3(1.0) - fresnel) * diffuseColour * (1.0 / kPi);
  terms.specular = fresnel * lobe;
  return terms;
}
)";
}

} // namespace outshine::Render
#endif
