#include "Wayfinding.h"
#include "Check.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  static_assert(!std::is_constructible_v<Path::Network, Path::Snap, Path::Sphere>);
  for (const double invalid : {0.0,
                               -1.0,
                               std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::denorm_min(),
                               std::numeric_limits<double>::max()}) {
    const auto cell = Path::Network::Create({.CellM = invalid}, {});
    const auto radius = Path::Network::Create({.CellM = 1}, {.RadiusM = invalid});
    CHECK(!cell && !cell.error().empty(), "invalid cell size has a diagnostic");
    CHECK(!radius && !radius.error().empty(), "invalid sphere has a diagnostic");
  }
  const double circumference = 2.0 * std::numbers::pi * kEarthMeanRadiusM;
  const double minimumCell =
      circumference / static_cast<double>(std::numeric_limits<uint32_t>::max());
  CHECK(!Path::Network::Create({.CellM = minimumCell / 2.0}, {}),
        "too many global columns cannot alias packed indices");
  for (const double cell : {minimumCell * 2.0, 1.0, circumference / 2.0}) {
    auto network = Path::Network::Create({.CellM = cell}, {});
    CHECK(network.has_value(), "representable fine and coarse grids accepted");
    if (!network) { continue; }
    CHECK(network->NodeCount() == 0 && network->WayCount() == 0 && network->SnapM() == cell,
          "creation preserves requested scale and starts empty");
  }
  CHECK(Path::Network::Create({.CellM = 0.1}, {.RadiusM = 1.0}).has_value(),
        "grid is not hardcoded to an Earth-sized sphere");
  return Report();
}
