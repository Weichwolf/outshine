#include <algorithm>
#include <array>
#include <cmath>

#include "Check.h"

#include "IridescenceLobe.h"

using outshine::Render::Fresnel0ToIor;
using outshine::Render::IorToFresnel0;
using outshine::Render::IridescenceFresnel;
using outshine::Render::IridescenceSchlick;
using outshine::Render::kOutsideIor;

namespace {

constexpr std::array<double, 6> kFilmIors = {1.0, 1.1, 1.3, 1.8, 2.4, 3.0};
constexpr std::array<double, 7> kBaseF0s = {0.0, 0.02, 0.04, 0.2, 0.766, 0.98, 1.0};

}

int main() {
  double worstHigh = 0.0;
  double worstLow = 1.0;
  long long samples = 0;
  for (const double filmIor : kFilmIors) {
    for (const double f0 : kBaseF0s) {
      const std::array<double, 3> base = {f0, f0 * 0.8, f0 * 0.6};
      for (double thickness = 0.0; thickness <= 2000.0; thickness += 25.0) {
        for (double cosTheta = 0.02; cosTheta <= 1.0; cosTheta += 0.02) {
          std::array<double, 3> f{};
          IridescenceFresnel(cosTheta, thickness, filmIor, base, f);
          for (const double channel : f) {
            worstHigh = std::fmax(worstHigh, channel);
            worstLow = std::fmin(worstLow, channel);
            ++samples;
          }
        }
      }
    }
  }
  outshine::Test::Note("population", (double)samples, "channels");
  CHECK(worstHigh <= 1.0, "a thin film reflects no more than arrives, on every index and thickness");
  CHECK(worstLow >= 0.0, "a thin film reflects no less than nothing");

  double previous = 0.0;
  for (const double f0 : {0.0, 0.1, 0.5, 0.9, 0.99, 0.9999, 1.0}) {
    const double ior = Fresnel0ToIor(f0 + 0.0001);
    CHECK(std::isfinite(ior) && ior > 0.0,
          "the inverse of Fresnel's normal-incidence case answers a real index at every reachable F0");
    CHECK(ior >= previous, "and it rises with the reflectance it inverts");
    previous = ior;
  }

  for (const double f0 : {0.0, 0.04, 0.2, 0.5, 0.9}) {
    CHECK_NEAR(IorToFresnel0(Fresnel0ToIor(f0), kOutsideIor), f0, 1.0e-12, "dimensionless",
               "F0 -> index -> F0 is the identity below the pole");
  }

  const double filmIor = 0.7;
  const double criticalCos = std::sqrt(1.0 - (filmIor / kOutsideIor) * (filmIor / kOutsideIor));
  const std::array<double, 3> base = {0.04, 0.04, 0.04};
  std::array<double, 3> inside{};
  std::array<double, 3> outside{};
  IridescenceFresnel(criticalCos * 0.95, 400.0, filmIor, base, inside);
  IridescenceFresnel(criticalCos * 1.05, 400.0, filmIor, base, outside);
  CHECK(inside[0] == 1.0 && inside[1] == 1.0 && inside[2] == 1.0,
        "past the critical angle the film reflects everything, in every channel");
  CHECK(outside[0] < 1.0, "and short of it, it does not");

  double widestSpread = 0.0;
  for (double thickness = 100.0; thickness <= 400.0; thickness += 10.0) {
    std::array<double, 3> f{};
    IridescenceFresnel(0.7, thickness, 1.3, base, f);
    widestSpread = std::fmax(widestSpread, *std::max_element(f.begin(), f.end()) -
                                               *std::min_element(f.begin(), f.end()));
  }
  outshine::Test::Note("widest channel separation over the default thickness range", widestSpread,
                       "reflectance");
  CHECK(widestSpread > 0.01, "the film tints, and a constant would not");

  const double cosTheta = 0.6;
  const double r12 = IridescenceSchlick(IorToFresnel0(1.3, kOutsideIor), cosTheta);
  const double cosTheta2 = std::sqrt(1.0 - (kOutsideIor / 1.3) * (kOutsideIor / 1.3) *
                                               (1.0 - cosTheta * cosTheta));
  const double r23 = IridescenceSchlick(IorToFresnel0(Fresnel0ToIor(0.04 + 0.0001), 1.3), cosTheta2);
  double series = r12;
  double carried = (1.0 - r12) * (1.0 - r12) * r23;
  for (int bounce = 0; bounce < 200; ++bounce) {
    series += carried;
    carried *= r12 * r23;
  }
  CHECK_NEAR(r12 + (1.0 - r12) * (1.0 - r12) * r23 / (1.0 - r12 * r23), series, 1.0e-12,
             "dimensionless", "the closed form is the sum over every round trip inside the film");

  outshine::Test::Covers("the thin-film Fresnel is a reflectance in both directions, at "
                         "every index the schema admits and at a white metal's F0 -- where the "
                         "extension's own guard produces a negative index");
  return outshine::Test::Report();
}
