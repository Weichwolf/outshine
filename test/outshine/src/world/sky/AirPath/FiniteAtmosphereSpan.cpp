#include "AirPath.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr double kCosineTolerance = 1e-6;
constexpr double kKilometresToMetres = 1000;

struct Case {
  float RadiusKm;
  float Cosine;
  float DistanceKm;
  float BeginKm;
  float EndKm;
};

void Verify(Case expected) {
  const AirRay ray = {.RadiusKm = expected.RadiusKm,
                      .CosView = expected.Cosine,
                      .CosSun = 0.25f,
                      .ViewSun = expected.Cosine * 0.25f,
                      .DistanceKm = expected.DistanceKm};
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
    CHECK_NEAR(
        clipped.CosView, -1.0, kCosineTolerance, "cosine", "radial entry still points inward");
    CHECK_NEAR(clipped.CosSun,
               ray.CosSun,
               kCosineTolerance,
               "cosine",
               "radial entry retains solar zenith");
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
  return end > start ? DoubleSpan{.Begin = start, .End = end} : DoubleSpan{};
}

void PhysicalRays() {
  constexpr double kMetreKm = 0.001;
  constexpr float kSolarCosine = 0.25f;
  for (const float radius : {kEarthAir.BottomRadiusKm + .005f,
                             kEarthAir.BottomRadiusKm + 2,
                             kEarthAir.TopRadiusKm + 1000}) {
    for (const float cosine : {-1.0f, -.99f, -.5f, -.1f, 0.0f, .5f, 1.0f}) {
      AirRay ray{.RadiusKm = radius,
                 .CosView = cosine,
                 .CosSun = kSolarCosine,
                 .ViewSun = 0,
                 .DistanceKm = 2 * (radius + kEarthAir.TopRadiusKm)};
      const auto expected = Reference(ray);
      const auto canonical = air_path::airPhysicalSpan(kEarthAir, ray);
      for (const float distance : {0.0f, .001f, .25f, 2.0f, 50.0f, 200.0f, 3000.0f}) {
        ray.DistanceKm = distance;
        const auto physical = air_path::airPhysicalSpan(kEarthAir, ray);
        CHECK_NEAR(physical.BeginKm,
                   expected.Begin,
                   kMetreKm,
                   "km",
                   "the physical atmosphere entry matches the independent sphere intersection");
        CHECK_NEAR(physical.EndKm,
                   expected.End,
                   kMetreKm,
                   "km",
                   "the physical endpoint stops at the first planet hit or atmosphere exit");
        CHECK(physical.BeginKm == canonical.BeginKm && physical.EndKm == canonical.EndKm,
              "opaque and thin query depths cannot change the physical source partition");
      }
    }
  }
}

void SlantedRays() {
  double maximumErrorKm = 0;
  constexpr double kMetreKm = 0.001;
  for (const float radius : {kEarthAir.BottomRadiusKm + .005f,
                             kEarthAir.BottomRadiusKm + 2,
                             kEarthAir.TopRadiusKm + 1000}) {
    for (const float cosine : {-1.0f, -.99f, -.5f, -.1f, 0.0f, .5f, 1.0f}) {
      for (const float distance : {.001f, .25f, 2.0f, 50.0f, 200.0f, 3000.0f}) {
        const AirRay ray = {.RadiusKm = radius,
                            .CosView = cosine,
                            .CosSun = 0.25f,
                            .ViewSun = 0,
                            .DistanceKm = distance};
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
  Note("maximum_entry_exit_error", maximumErrorKm * kKilometresToMetres, "m");
}

}

int main() {
  using namespace outshine;
  const float ground = kEarthAir.BottomRadiusKm;
  const float top = kEarthAir.TopRadiusKm;
  const std::array cases = {
      Case{.RadiusKm = ground + 2, .Cosine = 1, .DistanceKm = 200, .BeginKm = 0, .EndKm = 98},
      Case{.RadiusKm = ground + 2, .Cosine = -1, .DistanceKm = 200, .BeginKm = 0, .EndKm = 2},
      Case{.RadiusKm = ground + 2, .Cosine = -1, .DistanceKm = 0.5f, .BeginKm = 0, .EndKm = 0.5f},
      Case{.RadiusKm = ground + 2, .Cosine = 0, .DistanceKm = 200, .BeginKm = 0, .EndKm = 200},
      Case{
          .RadiusKm = top + 1000, .Cosine = -1, .DistanceKm = 3000, .BeginKm = 1000, .EndKm = 1100},
      Case{.RadiusKm = top + 1000, .Cosine = -1, .DistanceKm = 500, .BeginKm = 0, .EndKm = 0},
      Case{.RadiusKm = top + 1000, .Cosine = 1, .DistanceKm = 3000, .BeginKm = 0, .EndKm = 0},
      Case{.RadiusKm = top + 1000, .Cosine = 0, .DistanceKm = 3000, .BeginKm = 0, .EndKm = 0},
      Case{.RadiusKm = ground, .Cosine = 1, .DistanceKm = 200, .BeginKm = 0, .EndKm = 100},
      Case{.RadiusKm = ground, .Cosine = -1, .DistanceKm = 200, .BeginKm = 0, .EndKm = 0},
      Case{.RadiusKm = ground, .Cosine = 0, .DistanceKm = 200, .BeginKm = 0, .EndKm = 200},
      Case{.RadiusKm = top, .Cosine = 1, .DistanceKm = 200, .BeginKm = 0, .EndKm = 0},
      Case{.RadiusKm = top, .Cosine = -1, .DistanceKm = 200, .BeginKm = 0, .EndKm = 100},
      Case{.RadiusKm = top, .Cosine = 0, .DistanceKm = 200, .BeginKm = 0, .EndKm = 0},
      Case{.RadiusKm = ground - 1, .Cosine = 1, .DistanceKm = 200, .BeginKm = 0, .EndKm = 0},
      Case{.RadiusKm = ground + 2, .Cosine = 1, .DistanceKm = 0, .BeginKm = 0, .EndKm = 0},
  };
  for (const auto &expected : cases) { Verify(expected); }
  SlantedRays();
  PhysicalRays();
  return outshine::Test::Report();
}
