#include <array>
#include <limits>
#include "../../../src/base/curve/ReferenceLine.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 10}}, error), "initial line");
  const auto reject = [&](const Placed &from, std::initializer_list<Segment> segments) {
    CHECK(!line.Lay(from, segments, error), "invalid replacement rejected during construction");
    CHECK(!error.empty() && line.Error() == error, "failure reports its cause");
    Placed at;
    CHECK(line.LengthM() == 10 && line.At(5, at) && at.EastM == 5,
          "invalid construction preserves previous geometry");
  };
  constexpr std::array fields{&Placed::EastM,
                              &Placed::NorthM,
                              &Placed::HeightM,
                              &Placed::HeadingRad,
                              &Placed::CurvaturePerM,
                              &Placed::CurvatureRatePerM,
                              &Placed::Slope,
                              &Placed::SlopeRatePerM,
                              &Placed::BankRad,
                              &Placed::BankRatePerM};
  for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
    for (auto field : fields) {
      Placed from;
      from.*field = invalid;
      reject(from, {{.LengthM = 10}});
    }
    reject({}, {{.LengthM = invalid}});
    for (auto shape : {Curve::Straight, Curve::Arc, Curve::Spiral}) {
      reject({}, {{.Shape = shape, .LengthM = 10, .EntryCurvature = invalid}});
      reject({}, {{.Shape = shape, .LengthM = 10, .ExitCurvature = invalid}});
    }
  }
  reject({}, {{.Shape = static_cast<Curve>(255), .LengthM = 10}});
  reject({}, {{.LengthM = 0}});
  reject({}, {{.LengthM = -1}});
  const double maximum = std::numeric_limits<double>::max();
  reject({}, {{.LengthM = maximum}, {.LengthM = maximum}});
  reject({}, {{.LengthM = 1e20}, {.LengthM = 1}});
  reject({.EastM = maximum}, {{.LengthM = maximum}});
  reject({},
         {{.Shape = Curve::Spiral,
           .LengthM = 10,
           .EntryCurvature = -maximum,
           .ExitCurvature = maximum}});
  CHECK(line.Lay({}, {{.LengthM = 1e20}}, error), "large representable straight line");
  Placed at;
  CHECK(line.At(1e20, at) && at.EastM == 1e20, "large valid endpoint is retained");
  return Report();
}
