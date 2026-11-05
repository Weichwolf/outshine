#include "RoadDisplaySampler.h"
#include "Check.h"
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Generators;
  const Section section{.HalfWidthM = 2, .ShoulderM = 0.35, .ThicknessM = 0.3};
  const RoadDisplayLimits limits{.StationsMost = 4096, .PoseQueriesMost = 100000};
  ReferenceLine line;
  CHECK(!SampleRoadDisplay(line, section, 1, limits), "empty source has no product");
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 100}}, error), "source");
  for (const double bad : {0.0,
                           -1.0,
                           std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
    CHECK(!SampleRoadDisplay(line, section, bad, limits), "invalid error budget is rejected");
  }
  CHECK(!SampleRoadDisplay(line, section, 1, {.StationsMost = 1, .PoseQueriesMost = 1000}),
        "not even the endpoints fit an undersized station budget");
  CHECK(!SampleRoadDisplay(line, section, 1, {.StationsMost = 100, .PoseQueriesMost = 4}),
        "all source qualification and render evaluation must fit the query budget");
  CHECK(
      line.Lay({},
               {{.Shape = Curve::Arc, .LengthM = 100, .EntryCurvature = 0.1, .ExitCurvature = 0.1}},
               error),
      "curved source");
  CHECK(!SampleRoadDisplay(line, section, 0.001, {.StationsMost = 2, .PoseQueriesMost = 100000}),
        "failing a station budget does not publish a partial product");
  CHECK(!SampleRoadDisplay(line, {.HalfWidthM = 10, .ShoulderM = 1}, 1, limits),
        "offset folds cannot be admitted as coarse surfaces");
  CHECK(
      line.Lay(
          {},
          {{.Shape = Curve::Spiral, .LengthM = 1000, .EntryCurvature = 0.1, .ExitCurvature = 0.1}},
          error),
      "oscillating source");
  CHECK(!SampleRoadDisplay(line, section, 0.25, limits),
        "refining display geometry cannot fix inaccurate source quadrature");
  return Report();
}
