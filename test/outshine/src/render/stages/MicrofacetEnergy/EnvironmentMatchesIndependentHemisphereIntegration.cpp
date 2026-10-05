#include "Check.h"
#include "MicrofacetEnergy.h"
#include <array>
#include <cmath>
#include <numbers>

namespace {
std::array<double, 2> Integrate(double nv, double roughness) {
  constexpr int side = 512;
  const double a = roughness * roughness;
  const double a2 = a * a;
  const double vx = std::sqrt(1.0 - nv * nv);
  std::array<double, 2> sum{};
  for (int z = 0; z < side; ++z) {
    const double nl = (z + 0.5) / side;
    const double radius = std::sqrt(1.0 - nl * nl);
    for (int phi = 0; phi < side; ++phi) {
      const double angle = 2.0 * std::numbers::pi * (phi + 0.5) / side;
      const double lx = radius * std::cos(angle);
      const double ly = radius * std::sin(angle);
      const double length = std::sqrt((lx + vx) * (lx + vx) + ly * ly + (nl + nv) * (nl + nv));
      const double nh = (nl + nv) / length;
      const double vh = (vx * (lx + vx) + nv * (nl + nv)) / length;
      const double d = a2 / (std::numbers::pi * std::pow(nh * nh * (a2 - 1.0) + 1.0, 2.0));
      const double visibility = 0.5 / (nl * std::sqrt(nv * nv * (1.0 - a2) + a2) +
                                       nv * std::sqrt(nl * nl * (1.0 - a2) + a2));
      const double reflected = d * visibility * nl;
      sum[0] += reflected;
      sum[1] += reflected * std::pow(1.0 - vh, 5.0);
    }
  }
  for (double &value : sum) { value *= 2.0 * std::numbers::pi / (side * side); }
  return sum;
}
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  for (const double nv : {0.25, 0.5, 1.0}) {
    for (const double roughness : {0.4, 0.8, 1.0}) {
      const auto expected = Integrate(nv, roughness);
      const auto actual = GgxEnvironmentBrdf({.Cosine = nv, .Roughness = roughness});
      CHECK(std::abs(actual.Albedo - expected[0]) < 0.006,
            "importance sampling matches an independent uniform white-furnace integral");
      CHECK(std::abs(actual.FresnelBias - expected[1]) < 0.001,
            "the Fresnel basis matches independent direct BRDF integration");
      CHECK(actual.FresnelBias >= 0.0 && actual.FresnelBias <= actual.Albedo &&
                actual.Albedo <= 1.0,
            "both split-sum coefficients preserve nonnegative bounded energy");
    }
  }
  for (const double nv : {0.0, 0.1, 0.5, 0.9, 1.0}) {
    const auto actual = GgxEnvironmentBrdf({.Cosine = nv, .Roughness = 0.0});
    CHECK(actual.Albedo == 1.0 && std::abs(actual.FresnelBias - std::pow(1.0 - nv, 5.0)) < 1e-14,
          "the smooth limit is the analytic Schlick mirror response");
  }
  return Report();
}
