#include "StructureCellDetail.h"
#include "Check.h"

#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Ground::GeoBounds cell{
      .MinLonDeg = 9.0, .MinLatDeg = 47.0, .MaxLonDeg = 9.001, .MaxLatDeg = 47.001};
  const auto detail = [&](LongitudeLatitude eye, double focalPx = 720.0) {
    return RequestedStructureCellDetail(cell, 12.0f, 1000.0, eye, focalPx);
  };
  CHECK(detail({.LongitudeDeg = 9.0005, .LatitudeDeg = 47.0005}) == LevelOfDetail::Fine,
        "an eye inside the full footprint needs the finest variant");
  CHECK(detail({.LongitudeDeg = 9.02, .LatitudeDeg = 47.0}) == LevelOfDetail::Fine,
        "an unmeasured shell cannot replace a visible whole-cell footprint");
  CHECK(detail({.LongitudeDeg = 11.0, .LatitudeDeg = 47.0}) == LevelOfDetail::Shell,
        "a subpixel whole-cell extent may use the shell variant");
  CHECK(detail({.LongitudeDeg = 15.0, .LatitudeDeg = 47.0}) == LevelOfDetail::Massed,
        "a distant whole-cell extent may use the coarse variant");
  CHECK(detail({.LongitudeDeg = 11.0, .LatitudeDeg = 47.0}, 5000.0) == LevelOfDetail::Fine,
        "higher focal resolution requests more detail at the same position");
  const Ground::GeoBounds dateline{
      .MinLonDeg = 179.99, .MinLatDeg = 0, .MaxLonDeg = 180.01, .MaxLatDeg = 0.01};
  CHECK(RequestedStructureCellDetail(
            dateline, 12.0f, 1000, {.LongitudeDeg = -179.995, .LatitudeDeg = 0.005}, 720) ==
            LevelOfDetail::Fine,
        "a dateline crossing uses the local footprint instead of world-scale distance");
  CHECK(RequestedStructureCellDetail(cell,
                                     12.0f,
                                     1000,
                                     {.LongitudeDeg = 9.5, .LatitudeDeg = 47},
                                     std::numeric_limits<double>::quiet_NaN()) ==
            LevelOfDetail::Fine,
        "invalid projection data cannot silently lower structure detail");
  return Report();
}
