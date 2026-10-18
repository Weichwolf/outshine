#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include "Ribbon.h"
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
  const std::array required{0.0, 1.5, 3.0, 3.0, 10.0};
  const auto merged = Sweep(line, section, 0, 10, 3, RibbonForm::Surface, required);
  const std::array mergedStations{0.0, 1.5, 3.0, 6.0, 9.0, 10.0};
  CHECK(merged.Woven && merged.Stations == mergedStations.size(),
        "uniform and required stations form one ordered mesh without duplicate rings");
  if (merged.Woven) {
    for (size_t at = 0; at < mergedStations.size(); ++at) {
      CHECK_NEAR(merged.PositionM[at * kRibbonAcross * 3],
                 mergedStations[at],
                 0,
                 "meters",
                 "required geometric events retain their exact longitudinal position");
    }
  }
  for (const auto &invalid :
       std::array<std::vector<double>, 5>{{{4, 3}, {-1}, {11}, {nan}, {inf}}}) {
    CHECK(rejected(Sweep(line, section, 0, 10, 3, RibbonForm::Surface, invalid)),
          "invalid required stations are rejected before geometry allocation");
  }
  const std::vector<double> oversized(262145, 1.5);
  CHECK(rejected(Sweep(line, section, 0, 10, 3, RibbonForm::Surface, oversized)),
        "required-station input has an independent finite bound");
  CHECK(line.Rise(
            {{.AlongM = 0, .Value = 0}, {.AlongM = 1.5, .Value = 2}, {.AlongM = 10, .Value = 0}},
            error),
        "narrow analytic height crest is declared");
  const auto crest = Sweep(line, section, 0, 10, 32, RibbonForm::Surface, required);
  CHECK(crest.Woven && crest.Stations == 4,
        "height events remain present even between distant uniform samples");
  if (crest.Woven) {
    CHECK_NEAR(crest.PositionM[kRibbonAcross * 3 + 1],
               2,
               1e-6,
               "meters",
               "the required crest ring preserves its planned height");
  }
  return Report();
}
