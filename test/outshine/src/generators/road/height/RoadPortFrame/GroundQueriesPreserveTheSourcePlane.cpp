#include "Check.h"
#include "Geocentric.h"
#include "RoadPortFrame.h"
#include "TangentFrame.h"
#include "Units.h"
#include "Vec3.h"
#include <array>
#include <cmath>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckAnchor(LongitudeLatitude anchor) {
  const auto frame = TangentFrame::At(anchor);
  constexpr std::array shifts{EastNorth{},
                              EastNorth{.EastM = 100, .NorthM = 200},
                              EastNorth{.EastM = -10000, .NorthM = 20000},
                              EastNorth{.EastM = 1e6, .NorthM = -1e6}};
  for (const auto at : shifts) {
    const auto geographic = RoadPortGroundPosition(frame, at);
    CHECK(geographic.has_value(), "bounded source-plane rays reach the WGS84 surface");
    if (!geographic) { continue; }
    Vec3 point;
    GeoToEcef({.LongitudeDeg = geographic->LongitudeDeg, .LatitudeDeg = geographic->LatitudeDeg},
              point);
    const auto back = frame.ToLocalPosition(point);
    CHECK(std::abs(back.EastM - at.EastM) < 1e-8 && std::abs(back.NorthM - at.NorthM) < 1e-8,
          "DEM positions preserve the original local XY instead of using a flat degree estimate");
    const auto inverse = GeoFromEcef(point);
    CHECK(inverse && std::abs(inverse->HeightM) < 1e-8 &&
              std::abs(inverse->LatitudeDeg - geographic->LatitudeDeg) < 1e-10,
          "independent iterative geodesy confirms the returned surface position");
  }
}

void CheckEquator() {
  const auto frame = TangentFrame::At({});
  constexpr EastNorth at{.EastM = 1e6, .NorthM = 500000};
  const auto result = RoadPortGroundPosition(frame, at);
  const double b = kWgs84A * (1 - kWgs84F);
  const double x = kWgs84A * std::sqrt(1 - at.EastM * at.EastM / (kWgs84A * kWgs84A) -
                                       at.NorthM * at.NorthM / (b * b));
  CHECK(result && std::abs(result->LongitudeDeg - std::atan2(at.EastM, x) / kDeg2Rad) < 1e-12 &&
            std::abs(result->LatitudeDeg -
                     std::atan2(at.NorthM, (1 - kWgs84E2) * std::hypot(x, at.EastM)) / kDeg2Rad) <
                1e-12,
        "the far equatorial control agrees with a closed-form ellipsoid section");
  const auto invalid = RoadPortGroundPosition(frame, {.EastM = 1e6 + 1});
  CHECK(!invalid && invalid.error() == RoadPortQueryError::InvalidInput,
        "the source-frame recipe bounds are enforced before ray work");
  CHECK(!RoadPortGroundPosition(frame, {.EastM = std::numeric_limits<double>::quiet_NaN()}),
        "missing local coordinates cannot become invented terrain queries");
  CHECK(!RoadPortGroundPosition(TangentFrame::At({.LatitudeDeg = 91}), {}),
        "invalid source frames are rejected explicitly");
}
}

int main() {
  for (const auto anchor :
       std::array<LongitudeLatitude, 5>{{{},
                                         {.LongitudeDeg = 179.99, .LatitudeDeg = 48},
                                         {.LongitudeDeg = -179.99, .LatitudeDeg = -70},
                                         {.LongitudeDeg = 40, .LatitudeDeg = 90},
                                         {.LongitudeDeg = -100, .LatitudeDeg = -90}}}) {
    CheckAnchor(anchor);
  }
  CheckEquator();
  return Report();
}
