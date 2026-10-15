#include "AirPath.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct Case {
  float RadiusKm;
  float Cosine;
  float DistanceKm;
  float BeginKm;
  float EndKm;
};

void Verify(Case expected) {
  const AirRay ray = {
      expected.RadiusKm, expected.Cosine, 0.25f, expected.Cosine * 0.25f, expected.DistanceKm};
  const auto span = air_path::airPathSpan(kEarthAir, ray);
  constexpr double kMetreKm = 0.001;
  CHECK_NEAR(span.BeginKm, expected.BeginKm, kMetreKm, "km", "air entry is depth bounded");
  CHECK_NEAR(span.EndKm, expected.EndKm, kMetreKm, "km", "air exit stops at depth or first ground");
  CHECK(span.BeginKm >= 0.0f && span.EndKm >= span.BeginKm && span.EndKm <= expected.DistanceKm,
        "the air span is an ordered subset of the camera segment");
  const auto clipped = air_path::airPathRay(kEarthAir, ray, span);
  CHECK_NEAR(clipped.DistanceKm,
             span.EndKm - span.BeginKm,
             kMetreKm,
             "km",
             "vacuum before entry is excluded from air distance");
  CHECK(clipped.ViewSun == ray.ViewSun, "view-to-sun angle is constant through a straight ray");
  if (span.EndKm <= span.BeginKm) { return; }
  CHECK(clipped.RadiusKm >= kEarthAir.BottomRadiusKm && clipped.RadiusKm <= kEarthAir.TopRadiusKm,
        "the prefix starts inside its atmospheric domain");
  if (span.BeginKm > 0.0f) {
    CHECK(clipped.RadiusKm == kEarthAir.TopRadiusKm, "orbital air starts on the top shell");
    CHECK_NEAR(clipped.CosView, -1.0, 1e-6, "cosine", "radial entry still points inward");
    CHECK_NEAR(clipped.CosSun, ray.CosSun, 1e-6, "cosine", "radial entry retains solar zenith");
  }
}

struct DoubleSpan {
  double Begin = 0;
  double End = 0;
};

DoubleSpan Reference(AirRay ray) {
  const double radius = ray.RadiusKm;
  const double cosine = ray.CosView;
  const double projection = radius * cosine;
  const double impactSquared = radius * radius * (1.0 - cosine * cosine);
  const double top = kEarthAir.TopRadiusKm;
  const double topDiscriminant = top * top - impactSquared;
  if (topDiscriminant < 0) { return {}; }
  const double start = std::max(0.0, -projection - std::sqrt(topDiscriminant));
  double end =
      std::min(static_cast<double>(ray.DistanceKm), -projection + std::sqrt(topDiscriminant));
  const double ground = kEarthAir.BottomRadiusKm;
  const double groundDiscriminant = ground * ground - impactSquared;
  if (cosine < 0 && groundDiscriminant >= 0) {
    const double hit = -projection - std::sqrt(groundDiscriminant);
    if (hit >= 0) { end = std::min(end, hit); }
  }
  return end > start ? DoubleSpan{start, end} : DoubleSpan{};
}

void SlantedRays() {
  double maximumErrorKm = 0;
  constexpr double kMetreKm = 0.001;
  for (const float radius : {kEarthAir.BottomRadiusKm + .005f,
                             kEarthAir.BottomRadiusKm + 2,
                             kEarthAir.TopRadiusKm + 1000}) {
    for (const float cosine : {-1.0f, -.99f, -.5f, -.1f, 0.0f, .5f, 1.0f}) {
      for (const float distance : {.001f, .25f, 2.0f, 50.0f, 200.0f, 3000.0f}) {
        const AirRay ray = {radius, cosine, 0.25f, 0, distance};
        const auto reference = Reference(ray);
        const auto span = air_path::airPathSpan(kEarthAir, ray);
        const double errorKm = std::max(std::fabs(span.BeginKm - reference.Begin),
                                        std::fabs(span.EndKm - reference.End));
        maximumErrorKm = std::max(maximumErrorKm, errorKm);
        CHECK_NEAR(span.BeginKm,
                   reference.Begin,
                   kMetreKm,
                   "km",
                   "slanted top entry matches an independent double impact-parameter intersection");
        CHECK_NEAR(span.EndKm,
                   reference.End,
                   kMetreKm,
                   "km",
                   "slanted depth/ground/top termination matches the double intersection");
      }
    }
  }
  Note("maximum_entry_exit_error", maximumErrorKm * 1000, "m");
}

}

int main() {
  using namespace outshine;
  const float ground = kEarthAir.BottomRadiusKm;
  const float top = kEarthAir.TopRadiusKm;
  const std::array cases = {
      Case{ground + 2, 1, 200, 0, 98},
      Case{ground + 2, -1, 200, 0, 2},
      Case{ground + 2, -1, 0.5f, 0, 0.5f},
      Case{ground + 2, 0, 200, 0, 200},
      Case{top + 1000, -1, 3000, 1000, 1100},
      Case{top + 1000, -1, 500, 0, 0},
      Case{top + 1000, 1, 3000, 0, 0},
      Case{top + 1000, 0, 3000, 0, 0},
      Case{ground, 1, 200, 0, 100},
      Case{ground, -1, 200, 0, 0},
      Case{ground, 0, 200, 0, 200},
      Case{top, 1, 200, 0, 0},
      Case{top, -1, 200, 0, 100},
      Case{top, 0, 200, 0, 0},
      Case{ground - 1, 1, 200, 0, 0},
      Case{ground + 2, 1, 0, 0, 0},
  };
  for (const auto &expected : cases) { Verify(expected); }
  SlantedRays();
  return outshine::Test::Report();
}
