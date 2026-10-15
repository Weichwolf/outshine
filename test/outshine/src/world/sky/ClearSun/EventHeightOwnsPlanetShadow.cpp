#include "ClearSun.h"
#include "Check.h"
#include <algorithm>
#include <cmath>

namespace {
using namespace outshine;
using namespace outshine::Test;

bool VisibleDouble(float declaredRadius, float cosine, float lift) {
  const double radius = declaredRadius - lift;
  const double ground = kEarthAir.BottomRadiusKm;
  if (radius < ground) { return false; }
  const double projection = radius * cosine;
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
        CHECK(mediumSunVisible(kEarthAir, point, lift) == VisibleDouble(radius, cosine, lift),
              "planet visibility agrees with an independent double sphere intersection");
      }
      CHECK(mediumSunVisible(kEarthAir, {radius, 1.0f}, lift), "zenith sunlight leaves the planet");
      CHECK(!mediumSunVisible(kEarthAir, {radius, -1.0f}, lift),
            "nadir sunlight crosses the planet");
    }
  }
  const float sunsetCosine = std::sin(-2.0f * std::acos(-1.0f) / 180.0f);
  CHECK(!mediumSunVisible(kEarthAir, {kEarthAir.BottomRadiusKm, sunsetCosine}, 0.0f),
        "the ground is in planet shadow after sunset");
  CHECK(mediumSunVisible(kEarthAir, {kEarthAir.BottomRadiusKm + 5.0f, sunsetCosine}, 0.0f),
        "an elevated cloud may still receive sunlight above the ground shadow");
  CHECK(!mediumSunVisible(kEarthAir, {kEarthAir.BottomRadiusKm - 0.01f, 1.0f}, 0.0f),
        "points inside the planet never acquire an outgoing-sun visibility false positive");
  return Report();
}
