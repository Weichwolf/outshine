#include "TerrainLoader.h"
#include "Check.h"
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  for (const double invalid : {-1.0,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::max()}) {
    CHECK(!GroundPoolConfig({}, {.PatienceS = invalid}), "bad duration rejected before conversion");
  }
  const auto fractional = GroundPoolConfig({}, {.PatienceS = 0.125});
  CHECK(fractional && fractional->PollAttempts == 125,
        "one eighth second yields 125 millisecond polls");
  const auto defaults = GroundPoolConfig({});
  CHECK(defaults && defaults->PollAttempts == 0, "zero preserves pool-default selection");
  const double maximum = static_cast<double>(std::numeric_limits<int>::max()) / 1000.0;
  const auto boundary = GroundPoolConfig({}, {.PatienceS = maximum});
  CHECK(boundary && boundary->PollAttempts == std::numeric_limits<int>::max(),
        "largest poll count fits exactly");
  CHECK(!GroundPoolConfig(
            {}, {.PatienceS = std::nextafter(maximum, std::numeric_limits<double>::infinity())}),
        "next duration beyond count range is rejected");
  return Report();
}
