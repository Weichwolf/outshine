#ifndef OUTSHINE_RENDER_STAGES_MICROFACETENERGY_H
#define OUTSHINE_RENDER_STAGES_MICROFACETENERGY_H

#include "MetalRoughBrdf.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "ShaderFile.h"

#include "Units.h"

namespace outshine::Render {

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
    const double u1 = (i + 0.5) / kEnergySamples;
    unsigned bits = static_cast<unsigned>(i);
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    const double u2 = static_cast<double>(bits) * 2.3283064365386963e-10;

    const double cosH = std::sqrt((1.0 - u1) / (1.0 + (a2 - 1.0) * u1));
    const double sinH = std::sqrt(std::fmax(0.0, 1.0 - cosH * cosH));
    const double phi = 2.0 * kPi * u2;
    const double hx = sinH * std::cos(phi);
    const double hz = cosH;

    const double vh = sinV * hx + clampedNv * hz;
    if (!(vh > 0.0)) { continue; }

    const double lz = 2.0 * vh * hz - clampedNv;
    if (!(lz > 0.0)) { continue; }
    BrdfGeometry at;
    at.Nl = lz;
    at.Nv = clampedNv;
    at.Nh = hz;
    at.Vh = vh;

    total += 4.0 * lz * BrdfVisibility(lz, clampedNv, a2) * vh / hz;
  }
  return std::fmin(total / kEnergySamples, 1.0);
}

[[nodiscard]] inline double GgxEnergyAverage(double roughness) {
  double total = 0.0;
  for (int m = 0; m < kEnergyQuadrature; ++m) {
    const double mu = (m + 0.5) / kEnergyQuadrature;
    total += GgxDirectionalAlbedo(mu, roughness) * mu;
  }
  return std::fmin(2.0 * total / kEnergyQuadrature, 1.0);
}

[[nodiscard]] inline double SchlickAverage(double f0) {
  return f0 + (1.0 - f0) / 21.0;
}

inline void GgxEnergyScale(const std::array<double, 3> &f0,
                           double roughness,
                           double nv,
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

inline constexpr int kEnergyRoughnessSteps = 32;
inline constexpr int kEnergyViewSteps = 16;

[[nodiscard]] inline std::string MicrofacetEnergyMsl(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/microfacetEnergy.msl", body, error)) {
    return std::string();
  }
  std::string albedo;
  albedo.reserve(kEnergyRoughnessSteps * kEnergyViewSteps * 12);
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    const double roughness = static_cast<double>(r) / (kEnergyRoughnessSteps - 1);
    for (int v = 0; v < kEnergyViewSteps; ++v) {
      const double nv = static_cast<double>(v) / (kEnergyViewSteps - 1);
      char cell[32];
      std::snprintf(cell,
                    sizeof cell,
                    "%s%.6ff",
                    albedo.empty() ? "" : ", ",
                    GgxDirectionalAlbedo(nv, roughness));
      albedo += cell;
    }
  }
  std::string average;
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    char cell[32];
    std::snprintf(cell,
                  sizeof cell,
                  "%s%.6ff",
                  average.empty() ? "" : ", ",
                  GgxEnergyAverage(static_cast<double>(r) / (kEnergyRoughnessSteps - 1)));
    average += cell;
  }
  char head[256];
  std::snprintf(head,
                sizeof head,
                "constant int kEnergyRoughnessSteps = %d;\nconstant int kEnergyViewSteps = %d;\n",
                kEnergyRoughnessSteps,
                kEnergyViewSteps);
  return std::string(head) + "constant float kGgxAlbedo[] = { " + albedo + " };\n" +
         "constant float kGgxAlbedoAverage[] = { " + average + " };\n" + body;
}

[[nodiscard]] inline std::string MicrofacetEnergyMsl() {
  std::string ignored;
  return MicrofacetEnergyMsl(ignored);
}

} // namespace outshine::Render

#endif
