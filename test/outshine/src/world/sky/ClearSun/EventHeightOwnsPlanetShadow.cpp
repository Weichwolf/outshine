#include "ClearSun.h"
#include "Check.h"
#include <algorithm>
#include <cmath>

namespace {
using namespace outshine;
using namespace outshine::Test;

bool VisibleDouble(MediumLook point, float lift) {
  const double radius = point.RadiusKm - lift;
  const double ground = kEarthAir.BottomRadiusKm;
  if (radius < ground) { return false; }
  const double projection = radius * point.CosZenith;
  const double discriminant = projection * projection + (ground - radius) * (ground + radius);
  if (discriminant < 0.0) { return true; }
  return -projection - std::sqrt(discriminant) < 0.0;
}
}

int main() {
  using medium_core::mediumSunVisible;
  for (const float lift : {0.0f, kMediumGroundLiftKm}) {
    for (const float height : {0.0f, 0.001f, 0.5f, 2.0f, 20.0f, 100.0f, 1000.0f}) {
      const float radius = kEarthAir.BottomRadiusKm + height + lift;
      const double effectiveRadius = radius - lift;
      const double ground = kEarthAir.BottomRadiusKm;
      const double horizon =
          -std::sqrt(std::max(0.0, (effectiveRadius - ground) * (effectiveRadius + ground))) /
          effectiveRadius;
      for (const double offset : {-1e-3, -1e-4, -1e-6, 1e-6, 1e-4, 1e-3}) {
        const float cosine = static_cast<float>(std::clamp(horizon + offset, -1.0, 1.0));
        const MediumLook point = {.RadiusKm = radius, .CosZenith = cosine};
        CHECK(mediumSunVisible(kEarthAir, point, lift) == VisibleDouble(point, lift),
              "planet visibility agrees with an independent double sphere intersection");
      }
      CHECK(mediumSunVisible(kEarthAir, {radius, 1.0f}, lift), "zenith sunlight leaves the planet");
      CHECK(!mediumSunVisible(kEarthAir, {radius, -1.0f}, lift),
            "nadir sunlight crosses the planet");
    }
  }
  constexpr float kElevatedCloudKm = 5.0f;
  constexpr float kInsideGroundKm = 0.001f;
  const float sunsetCosine = std::sin(-2.0f * std::acos(-1.0f) / 180.0f);
  CHECK(!mediumSunVisible(kEarthAir, {kEarthAir.BottomRadiusKm, sunsetCosine}, 0.0f),
        "the ground is in planet shadow after sunset");
  CHECK(mediumSunVisible(
            kEarthAir, {kEarthAir.BottomRadiusKm + kElevatedCloudKm, sunsetCosine}, 0.0f),
        "an elevated cloud may still receive sunlight above the ground shadow");
  CHECK(!mediumSunVisible(kEarthAir, {kEarthAir.BottomRadiusKm - kMediumGroundLiftKm, 1.0f}, 0.0f),
        "points inside the planet never acquire an outgoing-sun visibility false positive");
  const MediumLook lowEvent = {.RadiusKm = kEarthAir.BottomRadiusKm + 0.005f, .CosZenith = 0.5f};
  CHECK(medium_core::mediumPhysicalSunVisible(kEarthAir, lowEvent),
        "a physical event five metres above ground sees the raised Sun");
  CHECK(!mediumSunVisible(kEarthAir, lowEvent, kMediumGroundLiftKm),
        "subtracting the legacy guard twice incorrectly puts that event inside the planet");
  const float nearHorizon = std::sin(-1.014f * std::acos(-1.0f) / 180.0f);
  const MediumLook highEvent = {.RadiusKm = kEarthAir.BottomRadiusKm + 1.0f,
                                .CosZenith = nearHorizon};
  CHECK(medium_core::mediumPhysicalSunVisible(kEarthAir, highEvent),
        "physical one kilometre event sees the Sun just above its sphere horizon");
  CHECK(!mediumSunVisible(kEarthAir, highEvent, kMediumGroundLiftKm),
        "a ten metre legacy subtraction shifts the physical shadow boundary");
  CHECK(!medium_core::mediumPhysicalSunVisible(
            kEarthAir, {.RadiusKm = kEarthAir.BottomRadiusKm - kInsideGroundKm, .CosZenith = 1.0f}),
        "the physical helper retains the inside-planet rejection");
  CHECK(!medium_core::mediumPhysicalSunVisible(
            kEarthAir, {.RadiusKm = kEarthAir.BottomRadiusKm, .CosZenith = -1.0f}),
        "the physical helper retains planet shadow");
  return Report();
}
