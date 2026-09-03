#ifndef OUTSHINE_RENDER_STAGES_MICROFACETENERGY_H
#define OUTSHINE_RENDER_STAGES_MICROFACETENERGY_H

#include "MetalRoughBrdf.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <string>

#include "ShaderFile.h"

#include "math/Units.h"

namespace outshine::Render {

constexpr uint32_t kEvenBits = 0x55555555u;
constexpr uint32_t kOddBits = 0xAAAAAAAAu;
constexpr uint32_t kPairsLow = 0x33333333u;
constexpr uint32_t kPairsHigh = 0xCCCCCCCCu;
constexpr uint32_t kNibblesLow = 0x0F0F0F0Fu;
constexpr uint32_t kNibblesHigh = 0xF0F0F0F0u;
constexpr uint32_t kBytesLow = 0x00FF00FFu;
constexpr uint32_t kBytesHigh = 0xFF00FF00u;
constexpr double kSchlickTail = 21.0;

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
    auto bits = static_cast<unsigned>(i);
    bits = std::rotl(bits, 16);
    bits = ((bits & kEvenBits) << 1u) | ((bits & kOddBits) >> 1u);
    bits = ((bits & kPairsLow) << 2u) | ((bits & kPairsHigh) >> 2u);
    bits = ((bits & kNibblesLow) << 4u) | ((bits & kNibblesHigh) >> 4u);
    bits = ((bits & kBytesLow) << 8u) | ((bits & kBytesHigh) >> 8u);
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

inline constexpr int kEnergyRoughnessSteps = 32;
inline constexpr int kEnergyViewSteps = 16;

[[nodiscard]] inline std::string MicrofacetEnergyMsl(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/microfacetEnergy.msl", body, error)) { return {}; }
  std::string albedo;
  albedo.reserve(size_t{kEnergyRoughnessSteps} * kEnergyViewSteps * 12);
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    const double roughness = static_cast<double>(r) / (kEnergyRoughnessSteps - 1);
    for (int v = 0; v < kEnergyViewSteps; ++v) {
      const double nv = static_cast<double>(v) / (kEnergyViewSteps - 1);
      std::array<char, 32> cell{};
      std::snprintf(cell.data(),
                    cell.size(),
                    "%s%.6ff",
                    albedo.empty() ? "" : ", ",
                    GgxDirectionalAlbedo(nv, roughness));
      albedo += cell.data();
    }
  }
  std::string average;
  for (int r = 0; r < kEnergyRoughnessSteps; ++r) {
    std::array<char, 32> cell{};
    std::snprintf(cell.data(),
                  cell.size(),
                  "%s%.6ff",
                  average.empty() ? "" : ", ",
                  GgxEnergyAverage(static_cast<double>(r) / (kEnergyRoughnessSteps - 1)));
    average += cell.data();
  }
  std::array<char, 256> head{};
  std::snprintf(head.data(),
                head.size(),
                "constant int kEnergyRoughnessSteps = %d;\nconstant int kEnergyViewSteps = %d;\n",
                kEnergyRoughnessSteps,
                kEnergyViewSteps);
  return std::string(head.data()) + "constant float kGgxAlbedo[] = { " + albedo + " };\n" +
         "constant float kGgxAlbedoAverage[] = { " + average + " };\n" + body;
}

[[nodiscard]] inline std::string MicrofacetEnergyMsl() {
  std::string ignored;
  return MicrofacetEnergyMsl(ignored);
}

} // namespace outshine::Render

#endif
