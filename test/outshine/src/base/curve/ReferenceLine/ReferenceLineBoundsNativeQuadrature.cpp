#include "ReferenceLine.h"
#include "Check.h"
#include <cmath>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ReferenceLine line;
  CHECK(!line.PlanarTruncationBoundM(0, 1), "an empty curve has no integration bound");
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 100}}, error), "straight source");
  CHECK(line.PlanarTruncationBoundM(0, 100) == 0,
        "straight evaluation has no quadrature truncation");
  CHECK(
      line.Lay({},
               {{.Shape = Curve::Arc, .LengthM = 100, .EntryCurvature = 0.1, .ExitCurvature = 0.1}},
               error),
      "closed arc source");
  CHECK(line.PlanarTruncationBoundM(0, 100) == 0,
        "closed arc evaluation has no quadrature truncation");
  CHECK(
      line.Lay(
          {},
          {{.Shape = Curve::Spiral, .LengthM = 1000, .EntryCurvature = 0.1, .ExitCurvature = 0.1}},
          error),
      "Gauss-evaluated arc source");
  for (const double distance : {10.0, 100.0, 1000.0}) {
    Placed actual;
    CHECK(line.At(distance, actual), "native integrated pose is available");
    const double exactEast = std::sin(0.1 * distance) / 0.1;
    const double exactNorth = (1 - std::cos(0.1 * distance)) / 0.1;
    const double observed = std::hypot(actual.EastM - exactEast, actual.NorthM - exactNorth);
    const auto bound = line.PlanarTruncationBoundM(0, distance);
    CHECK(bound && *bound >= 0, "the native Gauss8 truncation bound is available");
    CHECK(
        bound && observed <= *bound + 1e-12,
        "independent closed arc agrees within truncation plus explicit floating observation slack");
  }
  const auto prefix = line.PlanarTruncationBoundM(0, 900);
  const auto tail = line.PlanarTruncationBoundM(899, 900);
  CHECK(prefix && tail && *prefix == *tail,
        "a short tail still pays for integration from the original segment entry");
  CHECK(prefix && *prefix > 1,
        "long oscillatory source integration cannot claim submeter precision");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!line.PlanarTruncationBoundM(-1, 1) && !line.PlanarTruncationBoundM(2, 1) &&
            !line.PlanarTruncationBoundM(0, 1001) && !line.PlanarTruncationBoundM(0, nan),
        "invalid query domains have no bound");
  CHECK(
      line.Lay({},
               {{.Shape = Curve::Spiral, .LengthM = 10, .EntryCurvature = 0, .ExitCurvature = 0.1},
                {.Shape = Curve::Arc, .LengthM = 10, .EntryCurvature = 0.1, .ExitCurvature = 0.1}},
               error),
      "mixed source spans");
  CHECK(line.PlanarTruncationBoundM(0, 10).value_or(0) > 0, "spiral span is accounted for");
  CHECK(line.PlanarTruncationBoundM(10, 20) == 0,
        "an arc-only query excludes earlier truncation; stored pose drift remains a separate "
        "contract");
  CHECK(line.Lay(
            {},
            {{.Shape = Curve::Spiral, .LengthM = 1e-100, .EntryCurvature = 1, .ExitCurvature = 1}},
            error),
        "subnormal-remainder source");
  CHECK(line.PlanarTruncationBoundM(0, 1e-100).value_or(0) > 0,
        "positive mathematical truncation never rounds down to an exact zero bound");
  CHECK(line.Lay({}, {{.Shape = Curve::Spiral, .LengthM = 10}}, error), "zero-turn spiral source");
  CHECK(line.PlanarTruncationBoundM(0, 10) == 0, "zero derivative has exactly zero truncation");
  CHECK(line.Lay(
            {},
            {{.Shape = Curve::Spiral, .LengthM = 1e100, .EntryCurvature = 1, .ExitCurvature = 1}},
            error),
        "overflowing-remainder source");
  CHECK(std::isinf(line.PlanarTruncationBoundM(0, 1e100).value_or(0)),
        "overflowing majorants remain unbounded rather than certifying a finite error");
  return Report();
}
