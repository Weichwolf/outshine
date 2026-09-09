#include <limits>
#include "../../../src/base/curve/Ribbon.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (double turn : {-1.0, 1.0}) {
    ReferenceLine line;
    std::string error;
    CHECK(line.Lay({},
                   {{.Shape = Curve::Straight, .LengthM = 5},
                    {.Shape = Curve::Spiral,
                     .LengthM = 5,
                     .EntryCurvature = 0,
                     .ExitCurvature = turn * 0.5},
                    {.Shape = Curve::Spiral,
                     .LengthM = 5,
                     .EntryCurvature = turn * 0.5,
                     .ExitCurvature = 0},
                    {.Shape = Curve::Straight, .LengthM = 5}},
                   error),
          "continuous curvature hill");
    const auto maximum = line.MaxAbsCurvaturePerM(0, 20);
    CHECK(maximum && *maximum == 0.5, "peak curvature is present between sampling endpoints");
    const auto clipped = line.MaxAbsCurvaturePerM(5, 7);
    CHECK(clipped.has_value(), "clipped spiral interval is valid");
    if (clipped) {
      CHECK_NEAR(*clipped, 0.2, 1e-15, "1/m", "clipping interpolates the linear curvature");
    }
    CHECK(line.MaxAbsCurvaturePerM(0, 4).value_or(-1) == 0,
          "remote tight curves do not reject straight subintervals");
    for (double step : {0.5, 20.0}) {
      const auto folded = Sweep(line, {.HalfWidthM = 3, .ThicknessM = 0.5}, 0, 20, step);
      CHECK(!folded.Woven && !folded.Error.empty() && folded.PositionM.empty(),
            "fold is rejected before generating vertices, independently of station spacing");
    }
    CHECK(Sweep(line, {.HalfWidthM = 3, .ThicknessM = 0.5}, 0, 4, 1).Woven,
          "regular straight subset remains usable");
    CHECK(Sweep(line, {.HalfWidthM = 1, .ThicknessM = 0.5}, 0, 20, 0.5).Woven,
          "narrow offset remains locally regular");
    ReferenceLine arc;
    CHECK(arc.Lay({}, {{.Shape = Curve::Arc, .LengthM = 4, .EntryCurvature = turn * 0.25}}, error),
          "four-metre circle radius");
    CHECK(!Sweep(arc, {.HalfWidthM = 3, .ShoulderM = 1, .ThicknessM = 0.5}, 0, 4, 1).Woven,
          "outer shoulder reaching the centre of curvature is singular");
    CHECK(Sweep(arc, {.HalfWidthM = 3, .ThicknessM = 0.5}, 0, 4, 1).Woven,
          "width below curvature radius is supported");
    CHECK(!line.MaxAbsCurvaturePerM(-1, 2) && !line.MaxAbsCurvaturePerM(2, 1) &&
              !line.MaxAbsCurvaturePerM(0, 21) &&
              !line.MaxAbsCurvaturePerM(0, std::numeric_limits<double>::infinity()),
          "invalid query intervals cannot yield a valid bound");
  }
  ReferenceLine empty;
  CHECK(!empty.MaxAbsCurvaturePerM(0, 0), "absent reference geometry has no curvature bound");
  return Report();
}
