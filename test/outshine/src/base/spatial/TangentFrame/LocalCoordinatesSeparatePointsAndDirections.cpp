#include "Check.h"
#include "TangentFrame.h"

#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const LongitudeLatitude origin{.LongitudeDeg = 8.5, .LatitudeDeg = 47.2};
  const TangentFrame frame = TangentFrame::At(origin);
  const EastNorthUp atOrigin = frame.ToLocalPosition(frame.OriginEcef());
  CHECK(std::abs(atOrigin.EastM) < 1e-9 && std::abs(atOrigin.NorthM) < 1e-9 &&
            std::abs(atOrigin.UpM) < 1e-9,
        "the local origin is a position rather than an ECEF direction");

  const EastNorthUp east = frame.ToLocalDirection(frame.EastEcef());
  const EastNorthUp north = frame.ToLocalDirection(frame.NorthEcef());
  const EastNorthUp up = frame.ToLocalDirection(frame.UpEcef());
  CHECK(std::abs(east.EastM - 1.0) < 1e-12 && std::abs(east.NorthM) < 1e-12 &&
            std::abs(east.UpM) < 1e-12 && std::abs(north.EastM) < 1e-12 &&
            std::abs(north.NorthM - 1.0) < 1e-12 && std::abs(north.UpM) < 1e-12 &&
            std::abs(up.EastM) < 1e-12 && std::abs(up.NorthM) < 1e-12 &&
            std::abs(up.UpM - 1.0) < 1e-12,
        "ECEF basis directions remain orthonormal in the local frame");

  const EastNorthUp elevated = frame.ToLocalPosition(
      {.LongitudeDeg = origin.LongitudeDeg, .LatitudeDeg = origin.LatitudeDeg, .HeightM = 100.0});
  CHECK(std::abs(elevated.EastM) < 1e-8 && std::abs(elevated.NorthM) < 1e-8 &&
            std::abs(elevated.UpM - 100.0) < 1e-8,
        "geographic positions retain their height relative to the anchor");
  return Report();
}
