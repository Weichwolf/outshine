/* glTF 2.0's OWN metal-rough BRDF, term for term out of the specification's Appendix B, in C++ and
 * in MSL. The Khronos corpus states its criteria in this model, so an engine that shaded a house
 * approximation would be measuring the approximation.
 *
 * TWO HALVES OF ONE FORMULA: a shading term has to run on the device and has to be integrable on the
 * host, and no language spans both. The C++ half is the definition and the MSL half is its
 * transliteration. Three rules keep them from drifting: (a) no numeric constant is typed in the MSL
 * at all -- they are emitted from the C++ ones by `MetalRoughBrdfMsl()`; (b) the MSL declares no term
 * the C++ half does not, so a new term has to be written twice on purpose rather than once by
 * accident; (c) the ARRANGEMENT of the terms is measured -- `test/shader/BothHalvesOfTheBrdfAgree.cpp`
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
 * what the furnace sweep in test/unit/render/stages/ prints at roughness 0 and does not refuse. */
#ifndef METALROUGHBRDF_H
#define METALROUGHBRDF_H

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace outshine::Render {

/* glTF's dielectric normal-incidence reflectance at its default IOR of 1.5. */
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
                "constant float kPi = %.17g;\nconstant float kDielectricF0 = %.17g;\n", kBrdfPi,
                kDielectricF0);
  return std::string(constants) + R"(
struct Brdf { float3 diffuse; float3 specular; };

static inline float brdfDistribution(float nh, float a2) {
  float denominator = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (kPi * denominator * denominator);
}

static inline float brdfVisibility(float nl, float nv, float a2) {
  return 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
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
