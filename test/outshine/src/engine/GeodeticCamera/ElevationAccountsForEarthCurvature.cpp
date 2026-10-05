#include "Check.h"
#include "GeodeticCamera.h"
#include "math/Units.h"
#include <array>
#include <cmath>
#include <numbers>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double elevationM : std::array{0.0, 1772.0, 1000000.0}) {
    for (const double eastM : std::array{0.0, 100000.0, 240000.0}) {
      const auto position = GeographicPositionFor({{eastM, elevationM, 0}}, {});
      const double radiusM = kWgs84A + elevationM;
      const double expectedHeightM = std::hypot(radiusM, eastM) - kWgs84A;
      const double expectedLongitudeDeg = std::atan2(eastM, radiusM) * 180.0 / std::numbers::pi;
      CHECK_NEAR(position.HeightM,
                 expectedHeightM,
                 0.00001,
                 "ellipsoid elevation",
                 "the equatorial radial distance supplies an independent altitude oracle");
      CHECK_NEAR(position.LongitudeDeg,
                 expectedLongitudeDeg,
                 0.000000001,
                 "longitude",
                 "camera elevation and geographic focus share the same physical position");
      CHECK_NEAR(position.LatitudeDeg,
                 0,
                 0.000000001,
                 "latitude",
                 "eastward displacement remains on the equatorial plane");
    }
  }
  return Report();
}
