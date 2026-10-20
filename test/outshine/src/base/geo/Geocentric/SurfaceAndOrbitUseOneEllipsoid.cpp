#include "Geocentric.h"
#include "Check.h"
#include <algorithm>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double latitude :
       {-90., -89.999999999, -89.5, -45., 0., 45., 89.5, 89.999999999, 90.}) {
    for (const double longitude : {-180., -90., 0., 90., 180.}) {
      for (const double height : {-11000., -500., 0., 9000., 100000., 35786000., 1e9}) {
        const LongitudeLatitudeHeight source{
            .LongitudeDeg = longitude, .LatitudeDeg = latitude, .HeightM = height};
        Vec3 position;
        GeoToEcef(source, position);
        const auto geographic = GeoFromEcef(position);
        CHECK(geographic.has_value(), "surface, poles, dateline and orbital coordinates converge");
        if (!geographic) { continue; }
        Vec3 reconstructed;
        GeoToEcef(*geographic, reconstructed);
        const double scale = std::max(kWgs84A, Length(position));
        const double tolerance = 64 * std::numeric_limits<double>::epsilon() * scale;
        CHECK(Length(reconstructed - position) <= tolerance &&
                  std::abs(geographic->HeightM - height) <= tolerance &&
                  std::abs(geographic->LatitudeDeg - latitude) <= 1e-11,
              "inverse reconstruction and height retain a scale-aware Float64 bound");
      }
    }
  }
  const auto equator = GeoFromEcef(Vec3{{kWgs84A, 0, 0}});
  const auto pole = GeoFromEcef(Vec3{{0, 0, kWgs84A * (1 - kWgs84F)}});
  CHECK(equator && equator->HeightM == 0 && equator->LatitudeDeg == 0 && equator->LongitudeDeg == 0,
        "the equatorial analytic point is exact");
  CHECK(pole && std::abs(pole->HeightM) <= 1e-9 && pole->LatitudeDeg == 90 &&
            pole->LongitudeDeg == 0,
        "the axis uses a finite canonical longitude without dividing by cosine");
  CHECK(!GeoFromEcef(Vec3{{0, 0, 0}}) && !GeoFromEcef(Vec3{{1, 0, 0}}) &&
            !GeoFromEcef(Vec3{{std::numeric_limits<double>::quiet_NaN(), 0, 0}}) &&
            !GeoFromEcef(Vec3{{0, std::numeric_limits<double>::infinity(), 0}}),
        "ambiguous central coordinates and nonfinite inputs are explicitly rejected");
  return Report();
}
