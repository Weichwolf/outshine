#include "src/base/curve/ReferenceLine.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ReferenceLine line;
  std::string error;
  const auto prepare = [&] {
    CHECK(line.Lay({}, {{.LengthM = 10}}, error), "valid straight line");
    CHECK(line.Rise({{0, 3, 0.2}, {10, 5, 0.2}}, error), "height profile");
    CHECK(line.Bank({{0, 0.1, 0}, {10, 0.1, 0}}, error), "bank profile");
  };
  const auto preserved = [&] {
    CHECK(line.LengthM() == 10 && line.SegmentCount() == 1 && line.RiseKnotCount() == 2 &&
              line.BankKnotCount() == 2,
          "failed replacement preserves geometry and profiles");
    Placed at;
    CHECK(line.At(5, at), "previous line remains queryable");
    CHECK_NEAR(at.EastM, 5, 1e-12, "m", "straight station");
    CHECK_NEAR(at.HeightM, 4, 1e-12, "m", "preserved interpolated height");
    CHECK_NEAR(at.BankRad, 0.1, 1e-12, "rad", "preserved bank");
    CHECK(!error.empty() && line.Error() == error, "failure reports its cause");
  };
  prepare();
  CHECK(!line.Lay({}, {}, error), "empty replacement fails");
  preserved();
  prepare();
  CHECK(!line.Lay({}, {{.LengthM = 4}, {.LengthM = 0}}, error), "failure after staging a segment");
  preserved();
  prepare();
  CHECK(!line.Rise({{0, 8, 0}, {11, 8, 0}}, error), "out-of-range height fails");
  preserved();
  prepare();
  CHECK(!line.Bank({{0, 0.3, 0}, {0, 0.4, 0}}, error), "duplicate bank station fails");
  preserved();
  CHECK(line.Bank({}, error), "empty profile explicitly clears bank");
  CHECK(error.empty() && line.Error().empty(), "successful update clears stale diagnostics");
  CHECK(line.BankKnotCount() == 0 && line.RiseKnotCount() == 2,
        "successful update changes only its own profile");
  CHECK(line.Lay({}, {{.LengthM = 7}}, error), "valid replacement commits");
  CHECK(line.LengthM() == 7 && line.RiseKnotCount() == 0 && line.BankKnotCount() == 0,
        "new alignment resets profiles from the old station domain");
  return Report();
}
