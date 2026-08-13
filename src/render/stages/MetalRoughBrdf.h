/* glTF 2.0's OWN metal-rough BRDF, term for term out of the specification's Appendix B, in C++ and
 * in WGSL. The Khronos corpus states its criteria in this model, so an engine that shaded a house
 * approximation would be measuring the approximation.
 *
 * TWO HALVES OF ONE FORMULA, the same arrangement `CloudDensityWGSL.h` carries and for the same
 * reason: a shading term has to run on the device and has to be integrable on the host, and no
 * language spans both. The C++ half is the definition and the WGSL half is its transliteration.
 * Two rules keep them from drifting: (a) no numeric constant is typed in the WGSL at all -- they are
 * emitted from the C++ ones by `MetalRoughBrdfWGSL()`; (b) the WGSL declares no term the C++ half
 * does not, so a new term has to be written twice on purpose rather than once by accident.
 * WHAT REMAINS UNCHECKED IS THE ARRANGEMENT OF THE TERMS, and it is unchecked: nothing in this tree
 * evaluates both halves over a sample set and compares them.
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
 * one at the facet that actually reflects V into L. */
[[nodiscard]] inline std::array<double, 3> BrdfFresnel(const std::array<double, 3> &f0, double vh) {
  const double weight = std::pow(1.0 - vh, 5.0);
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

/* The device half. A textual splice, like `AtmoCommon.h` -- never compiled alone. */
[[nodiscard]] inline std::string MetalRoughBrdfWGSL(void) {
  char constants[256];
  std::snprintf(constants, sizeof constants,
                "const kPi : f32 = %.17g;\nconst kDielectricF0 : f32 = %.17g;\n", kBrdfPi,
                kDielectricF0);
  return std::string(constants) + R"(
struct Brdf { diffuse : vec3f, specular : vec3f };

fn brdfDistribution(nh : f32, a2 : f32) -> f32 {
  let denominator = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (kPi * denominator * denominator);
}

fn brdfVisibility(nl : f32, nv : f32, a2 : f32) -> f32 {
  return 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
}

fn brdfFresnel(f0 : vec3f, vh : f32) -> vec3f {
  return f0 + (vec3f(1.0) - f0) * pow(1.0 - vh, 5.0);
}

fn metalRoughBrdf(diffuseColour : vec3f, f0 : vec3f, a2 : f32,
                  nl : f32, nv : f32, nh : f32, vh : f32) -> Brdf {
  let fresnel = brdfFresnel(f0, vh);
  var out : Brdf;
  var lobe = 0.0;
  if (a2 > 0.0) { lobe = brdfDistribution(nh, a2) * brdfVisibility(nl, nv, a2); }
  out.diffuse = (vec3f(1.0) - fresnel) * diffuseColour * (1.0 / kPi);
  out.specular = fresnel * lobe;
  return out;
}
)";
}

} // namespace outshine::Render
#endif
