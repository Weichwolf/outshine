/* THE ENERGY SINGLE-SCATTER GGX LOSES, AND THE LOBE THAT PUTS IT BACK (board:1408).
 *
 * A microfacet model that traces ONE bounce off the facets loses everything that would have left after
 * two or more, and the loss grows with roughness because a rougher surface shadows itself more. glTF's
 * Appendix B specifies exactly that one bounce; the core specification's own normative text says an
 * implementation of the BRDF **MAY** vary, so restoring the missing energy is inside the format rather
 * than a departure from it.
 *
 * THE MEASUREMENT THAT MOTIVATED IT. `shaded-sphere-metal` -- one sphere, one conductor, one light --
 * came back 5.8 % darker than Cycles at the median and 8.7 % at p99, with NO angular feature: p50, p95
 * and p99 within a factor of 1.5 of each other where the dielectric sibling spans a factor of 16. The
 * same case at roughness 0 is bit-identical to the oracle, 0 channels differing. **The term that
 * vanishes with the microfacets is the term that was missing.**
 *
 * KULLA AND CONTY, *Revisiting Physically Based Shading at Imageworks*, SIGGRAPH 2017. The compensation
 * is a second lobe whose shape is fixed by the directional albedo of the first, and it has NO free
 * parameter -- which is what makes it a derived correction and not a fit to the residual above.
 *
 * THE ALBEDO IS INTEGRATED FROM OUR OWN LOBE AND NOT FETCHED, the same decision `SheenLobe.h` records
 * and for the same reason: a copied table can drift away from the lobe it belongs to the moment the
 * lobe is touched, and an integral over `BrdfLobe` cannot. */
#ifndef MICROFACETENERGY_H
#define MICROFACETENERGY_H

#include "MetalRoughBrdf.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace outshine::Render {

/* THE WHITE-FURNACE DIRECTIONAL ALBEDO OF `BrdfLobe`: how much of the light arriving from a uniform
 * environment a PERFECTLY reflecting version of this lobe sends back. Fresnel is 1 here on purpose --
 * the colour enters later, through the average Fresnel, and a table that carried F0 would need a third
 * dimension for a quantity that factors out.
 *
 * SAMPLED FROM THE DISTRIBUTION AND NOT ON A GRID, AND THE GRID IS WHY. A uniform quadrature over the
 * light hemisphere was written first and it is WRONG where it matters most: at roughness 0.10 it
 * returned [MEASURED] `E(0.5) = 0.22156` and `E(0.1) = 0.01606` for a surface that is nearly a mirror,
 * and at roughness 0.5 it returned `E(0.1) = 0.886` against `E(0.5) = 0.857` -- not even monotone. A
 * narrow lobe falls between the samples, and every one of those numbers would have become a
 * COMPENSATION of the missing energy: a smooth surface brightened by a factor of five. *Caught by
 * looking at the table rather than at the case it was built for.*
 *
 * WHAT THE CANCELLATION COSTS, STATED. Sampling the distribution puts `D` in the numerator and in the
 * density, so it cancels and this integral is blind to a constant factor inside `BrdfDistribution` --
 * which is exactly the blindness `TheMicrofacetLobeAddsNoEnergy` was built with an independent sampler
 * to avoid. That test guards the normalisation; this one needs the estimator to AGREE with the lobe
 * rather than to audit it, and the two instruments are pointed at different questions on purpose. */
/* The average is a one-dimensional quadrature over the albedo above, which is smooth in `mu`. */
inline constexpr int kEnergyQuadrature = 64;
inline constexpr int kEnergySamples = 2048;

[[nodiscard]] inline double GgxDirectionalAlbedo(double nv, double roughness) {
  const double alpha = roughness * roughness;
  const double a2 = alpha * alpha;
  if (!(a2 > 0.0)) { return 1.0; }
  const double clampedNv = std::fmax(nv, 1.0e-4);
  const double sinV = std::sqrt(std::fmax(0.0, 1.0 - clampedNv * clampedNv));
  double total = 0.0;
  for (int i = 0; i < kEnergySamples; ++i) {
    /* Hammersley, so the table is the same on every run and a difference between two rounds is a
     * change in the lobe rather than in a seed. */
    const double u1 = (i + 0.5) / kEnergySamples;
    unsigned bits = (unsigned)i;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    const double u2 = (double)bits * 2.3283064365386963e-10;
    /* GGX's own normal distribution, sampled by its inverse: `tan^2(theta) = a2 u / (1 - u)`. */
    const double cosH = std::sqrt((1.0 - u1) / (1.0 + (a2 - 1.0) * u1));
    const double sinH = std::sqrt(std::fmax(0.0, 1.0 - cosH * cosH));
    const double phi = 2.0 * kBrdfPi * u2;
    const double hx = sinH * std::cos(phi);
    const double hz = cosH;
    /* The azimuth turns the facet out of the view plane and the view has no y component, so `h.y`
     * enters `v.h` with a coefficient of zero and is not formed. */
    const double vh = sinV * hx + clampedNv * hz;
    if (!(vh > 0.0)) { continue; }
    /* The light is the view reflected about the sampled facet. */
    const double lz = 2.0 * vh * hz - clampedNv;
    if (!(lz > 0.0)) { continue; }
    BrdfGeometry at;
    at.Nl = lz;
    at.Nv = clampedNv;
    at.Nh = hz;
    at.Vh = vh;
    /* `E = mean[ 4 n.l V(n.l, n.v) (v.h) / (n.h) ]`: the lobe over the density, with `D` cancelled and
     * the half-vector Jacobian `1 / (4 v.h)` folded in. */
    total += 4.0 * lz * BrdfVisibility(lz, clampedNv, a2) * vh / hz;
  }
  return std::fmin(total / kEnergySamples, 1.0);
}

/* THE COSINE-WEIGHTED AVERAGE OF THE ABOVE OVER THE HEMISPHERE -- `2 * integral E(mu) mu dmu` -- which
 * is what a second bounce sees, because a facet that scattered once is illuminated from everywhere. */
[[nodiscard]] inline double GgxEnergyAverage(double roughness) {
  double total = 0.0;
  for (int m = 0; m < kEnergyQuadrature; ++m) {
    const double mu = (m + 0.5) / kEnergyQuadrature;
    total += GgxDirectionalAlbedo(mu, roughness) * mu;
  }
  return std::fmin(2.0 * total / kEnergyQuadrature, 1.0);
}

/* THE AVERAGE OF SCHLICK'S FRESNEL OVER THE HEMISPHERE, IN CLOSED FORM AND NOT AS A THIRD TABLE.
 * `2 * integral (F0 + (1 - F0)(1 - mu)^5) mu dmu` = `F0 + (1 - F0) / 21`, because
 * `2 * integral mu (1 - mu)^5 dmu` over [0, 1] is `2 * (1/6 - 1/7)` = `1/21`. */
[[nodiscard]] inline double SchlickAverage(double f0) { return f0 + (1.0 - f0) / 21.0; }

/* THE COMPENSATION IS A MULTIPLIER ON THE LOBE AND NOT A SECOND LOBE BESIDE IT, AND THAT WAS DECIDED
 * BY A MEASUREMENT THAT REFUTED THE OTHER SHAPE (board:1408).
 *
 * Kulla and Conty give the missing energy as an added lobe, `(1 - E(mu_o))(1 - E(mu_i)) / (pi (1 -
 * E_avg))`, which spreads it evenly over the hemisphere. **That was built first and it overshot by a
 * factor of four**: [MEASURED] `shaded-sphere-metal` went from 5.8 % dark at the median to 29.6 %
 * BRIGHT, and its p99 from 8.7 % to 72.8 %. An added lobe is comparable to the single-scatter term
 * where that term is strong and dwarfs it where the surface is dim, so a per-pixel relative error that
 * was FLAT -- p50 0.058 against p99 0.087, a factor of 1.5 -- cannot be what an added lobe explains.
 * *The residual's own shape said the correction was multiplicative before any of this was written, and
 * reading it off the data is what the round should have done first.*
 *
 * The multiplier form is the same paper's, and it keeps the single-scatter lobe's SHAPE while
 * restoring its total: `1 + F_ms (1 - E) / E`, with `F_ms = F_avg E_avg / (1 - F_avg (1 - E_avg))`.
 * One `F_avg` rather than two, because the term multiplies a BSDF that already carries one bounce of
 * Fresnel.
 *
 * IT DEPENDS ON THE VIEW ALONE. `E` is read at the view's own cosine and at no light's, so this is a
 * fragment constant and leaves the light loop -- which is why the frame path pays for it once per
 * pixel rather than once per light.
 *
 * A LOSSLESS LOBE MULTIPLIES BY ONE, AS ARITHMETIC RATHER THAN AS A GUARD. At roughness 0 the albedo
 * is 1, so `(1 - E) / E` is 0 and the multiplier is exactly 1 -- which is what keeps the case that
 * measures roughness 0 bit-identical. */
inline void GgxEnergyScale(const std::array<double, 3> &f0, double roughness, double nv,
                           std::array<double, 3> &out) {
  const double e = GgxDirectionalAlbedo(nv, roughness);
  if (!(e > 0.0) || !(e < 1.0)) {
    out = {1.0, 1.0, 1.0};
    return;
  }
  const double missing = (1.0 - e) / e;
  const double eAverage = GgxEnergyAverage(roughness);
  for (std::size_t channel = 0; channel < 3; ++channel) {
    const double favg = SchlickAverage(f0[channel]);
    const double fms = favg * eAverage / (1.0 - favg * (1.0 - eAverage));
    out[channel] = 1.0 + fms * missing;
  }
}

/* THE DEVICE HALF. The tables are the integrals' own output, written into the shader text, so the two
 * halves cannot state different numbers.
 *
 * SAMPLED AT THE EDGES AND READ BY INTERPOLATION, WHICH IS WHERE THIS DIFFERS FROM `SheenLobe.h`'s
 * table. Sheen reads its nearest cell because its albedo varies slowly and no case stands on an exact
 * value. This one does: at roughness 0 the compensation must be EXACTLY zero, and a cell centre at
 * `1 / 2N` would return an albedo below 1 and brighten a mirror. The grid therefore includes both ends
 * and the lookup is bilinear, so `roughness = 0` lands on the sample that is 1 by construction. */
inline constexpr int kEnergyRoughnessSteps = 32;
inline constexpr int kEnergyViewSteps = 16;

[[nodiscard]] inline std::string MicrofacetEnergyMsl(void) {
  std::string albedo;
  albedo.reserve(kEnergyRoughnessSteps * kEnergyViewSteps * 12);
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    const double roughness = (double)r / (kEnergyRoughnessSteps - 1);
    for (int v = 0; v < kEnergyViewSteps; ++v) {
      const double nv = (double)v / (kEnergyViewSteps - 1);
      char cell[32];
      std::snprintf(cell, sizeof cell, "%s%.6ff", albedo.empty() ? "" : ", ",
                    GgxDirectionalAlbedo(nv, roughness));
      albedo += cell;
    }
  }
  std::string average;
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    char cell[32];
    std::snprintf(cell, sizeof cell, "%s%.6ff", average.empty() ? "" : ", ",
                  GgxEnergyAverage((double)r / (kEnergyRoughnessSteps - 1)));
    average += cell;
  }
  char head[256];
  std::snprintf(head, sizeof head,
                "constant int kEnergyRoughnessSteps = %d;\nconstant int kEnergyViewSteps = %d;\n",
                kEnergyRoughnessSteps, kEnergyViewSteps);
  return std::string(head) + "constant float kGgxAlbedo[] = { " + albedo + " };\n" +
         "constant float kGgxAlbedoAverage[] = { " + average + " };\n" + R"(
static inline float ggxDirectionalAlbedo(float nv, float roughness) {
  float rf = clamp(roughness, 0.0, 1.0) * float(kEnergyRoughnessSteps - 1);
  float vf = clamp(nv, 0.0, 1.0) * float(kEnergyViewSteps - 1);
  int r0 = int(rf); int v0 = int(vf);
  int r1 = min(r0 + 1, kEnergyRoughnessSteps - 1);
  int v1 = min(v0 + 1, kEnergyViewSteps - 1);
  float rt = rf - float(r0); float vt = vf - float(v0);
  float a = mix(kGgxAlbedo[r0 * kEnergyViewSteps + v0], kGgxAlbedo[r0 * kEnergyViewSteps + v1], vt);
  float b = mix(kGgxAlbedo[r1 * kEnergyViewSteps + v0], kGgxAlbedo[r1 * kEnergyViewSteps + v1], vt);
  return mix(a, b, rt);
}

static inline float ggxEnergyAverage(float roughness) {
  float rf = clamp(roughness, 0.0, 1.0) * float(kEnergyRoughnessSteps - 1);
  int r0 = int(rf); int r1 = min(r0 + 1, kEnergyRoughnessSteps - 1);
  return mix(kGgxAlbedoAverage[r0], kGgxAlbedoAverage[r1], rf - float(r0));
}

static inline float3 schlickAverage(float3 f0) { return f0 + (1.0 - f0) / 21.0; }

static inline float3 ggxEnergyScale(float3 f0, float roughness, float nv) {
  float e = ggxDirectionalAlbedo(nv, roughness);
  if (!(e > 0.0) || !(e < 1.0)) { return float3(1.0); }
  float missing = (1.0 - e) / e;
  float eAverage = ggxEnergyAverage(roughness);
  float3 favg = schlickAverage(f0);
  float3 fms = favg * eAverage / (1.0 - favg * (1.0 - eAverage));
  return 1.0 + fms * missing;
}
)";
}

} // namespace outshine::Render

#endif
