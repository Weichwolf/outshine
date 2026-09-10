#include "src/base/curve/Alignment.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<double, 6> straight{0, 0, 10, 0, 20, 0};
  ReferenceLine line;
  const Aligned noBends;
  CHECK(Align(straight, 1, 1).has_value(), "straight coordinates are accepted for fitting");
  CHECK(LayAligned(straight, noBends, line).has_value() && line.LengthM() == 20,
        "straight coordinates produce their analytic length");
  const auto reject = [&](std::span<const double> points) {
    CHECK(!Align(points, 1, 1), "invalid coordinates are refused by fitting");
    CHECK(!LayAligned(points, noBends, line), "invalid coordinates are refused by laying");
    CHECK(line.LengthM() == 20, "rejected coordinates retain the previous reference line");
  };
  const std::array<double, 7> odd{0, 0, 10, 0, 20, 0, 99};
  reject(odd);
  for (size_t at = 0; at < straight.size(); ++at) {
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()}) {
      auto points = straight;
      points[at] = invalid;
      reject(points);
    }
  }
  return Report();
}
