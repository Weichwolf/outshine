#include "Check.h"
#include "HipRoofPlan.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<EastNorth, 8> ring{{{0, 0},
                                       {-0.45231913453437711, 0.45155481886627058},
                                       {-0.90463826906875378, -0.45155484650368294},
                                       {-2.2615956726718842, 0.45155481886627058},
                                       {3.1662339417406367, 9.9341996371328207},
                                       {-7.6894252870844051, 15.804402235010485},
                                       {-12.255186343214582, 7.1026908842675827},
                                       {-0.41524366068275098, -0.7772666189143429}}};
  const auto result = HipRoofPlan::Prepare(ring, {}, {});
  CHECK(!result && result.error() == HipRoofError::InsufficientClearance,
        "formally valid near-touching contours are refused before unbounded skeleton cleanup");
  return Report();
}
