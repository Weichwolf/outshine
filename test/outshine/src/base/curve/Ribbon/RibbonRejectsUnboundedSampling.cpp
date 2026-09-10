#include <array>
#include <cmath>
#include <limits>
#include "src/base/curve/Ribbon.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 10}}, error), "independent straight reference line");
  const Section section{.HalfWidthM = 2, .ShoulderM = 1, .ThicknessM = 0.5};
  const auto valid = Sweep(line, section, 0, 10, 3);
  CHECK(valid.Woven && valid.Stations == 5, "stations at 0, 3, 6, 9 and exact endpoint 10");
  CHECK(valid.Vertices == 5 * 12 + 16 && valid.Triangles == 4 * 16 + 12,
        "four cross-section vertices, separate walls and two end caps");
  constexpr std::array<double, 5> expectedStations = {0, 3, 6, 9, 10};
  if (valid.Woven) {
    for (size_t station = 0; station < 5; ++station) {
      CHECK_NEAR(valid.PositionM[station * 12 * 3],
                 expectedStations[station],
                 0,
                 "m",
                 "straight ribbon station follows analytic east position");
    }
    for (uint32_t index : valid.Index) {
      CHECK(index < valid.Vertices, "indices reference owned vertices");
    }
  }
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const auto rejected = [](const Ribbon &r) {
    return !r.Woven && !r.Error.empty() && r.PositionM.empty() && r.Index.empty();
  };
  for (double invalid : {inf, nan, -1.0, 0.0}) {
    CHECK(rejected(Sweep(line, section, 0, 10, invalid)),
          "invalid sampling step rejected before generation");
    auto bad = section;
    bad.HalfWidthM = invalid;
    CHECK(rejected(Sweep(line, bad, 0, 10, 1)), "invalid width rejected");
    bad = section;
    bad.ThicknessM = invalid;
    CHECK(rejected(Sweep(line, bad, 0, 10, 1)), "invalid thickness rejected");
  }
  for (double invalid : {inf, nan, -1.0}) {
    auto bad = section;
    bad.ShoulderM = invalid;
    CHECK(rejected(Sweep(line, bad, 0, 10, 1)), "invalid shoulder rejected");
  }
  CHECK(rejected(Sweep(line, section, 0, inf, 1)), "infinite endpoint rejected");
  CHECK(rejected(Sweep(line, section, -inf, 10, 1)), "infinite start rejected");
  CHECK(rejected(Sweep(line, section, 0, 10, std::numeric_limits<double>::denorm_min())),
        "overflowing station quotient rejected before integer conversion");
  CHECK(rejected(Sweep(line, section, 0, 10, 1e-8)), "finite oversized station count rejected");
  return Report();
}
