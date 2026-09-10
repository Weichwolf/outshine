#include <limits>
#include "src/base/curve/ReferenceLine.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 10}}, error), "straight domain");
  CHECK(line.Rise({{0, 3, 0}}, error), "constant height");
  CHECK(line.Bank({{0, 0.1, 0}}, error), "constant bank");
  for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
    for (const Knot knot : {Knot{invalid, 0, 0}, Knot{0, invalid, 0}, Knot{0, 0, invalid}}) {
      CHECK(!line.Rise({knot}, error), "nonfinite height knot rejected");
      CHECK(!error.empty(), "height rejection has a diagnostic");
      CHECK(!line.Bank({knot}, error), "nonfinite bank knot rejected");
      Placed at;
      CHECK(line.At(5, at), "previous profiles remain usable");
      CHECK(at.HeightM == 3 && at.BankRad == 0.1, "previous values are preserved");
    }
    Placed at{.EastM = 42, .NorthM = 43, .HeightM = 44};
    CHECK(!line.At(invalid, at), "nonfinite station rejected");
    CHECK(at.EastM == 42 && at.NorthM == 43 && at.HeightM == 44,
          "failed query preserves caller output");
  }
  const double maximum = std::numeric_limits<double>::max();
  CHECK(line.Rise({{0, maximum, maximum}}, error), "finite single-knot profile");
  Placed at{.EastM = 42, .NorthM = 43, .HeightM = 44};
  CHECK(!line.At(5, at), "unrepresentable extrapolation is an explicit query failure");
  CHECK(at.EastM == 42 && at.NorthM == 43 && at.HeightM == 44,
        "overflow does not publish a partial pose");
  CHECK(line.At(0, at) && at.HeightM == maximum,
        "representable station on the same profile remains queryable");
  return Report();
}
