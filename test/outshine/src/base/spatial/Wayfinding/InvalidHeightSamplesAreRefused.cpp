#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double infinity = std::numeric_limits<double>::infinity();
  const std::array<std::optional<double>, 6> samples{
      std::nullopt, std::numeric_limits<double>::quiet_NaN(), infinity, -infinity, -25.0, 75.0};
  for (const bool woven : {false, true}) {
    for (const auto sample : samples) {
      auto created = Path::Network::Create({.CellM = 1}, {});
      CHECK(created.has_value(), "valid network");
      if (!created) { return Report(); }
      auto &network = *created;
      const std::array<double, 4> points{0, 0, 0.01, 0};
      CHECK(network.Lay(points, {}).has_value(), "two-point way accepted");
      std::string error;
      if (woven) { CHECK(network.Weave(error), "network woven"); }
      const auto elevated = network.Elevate([sample](LongitudeLatitude) { return sample; });
      const bool valid = sample && std::isfinite(*sample);
      CHECK(elevated.Points == (valid ? 2U : 0U) && elevated.Refused == (valid ? 0U : 2U),
            "only finite samples count as accepted");
      CHECK(elevated.SteepestGrade == 0, "constant or missing field has zero grade");
      for (const double fraction : {0.0, 0.5, 1.0}) {
        const auto profile = network.Profile({.Way = 0, .StationM = fraction * network.LengthM(0)});
        CHECK(profile.has_value(), "profile available");
        if (profile) {
          CHECK(profile->HeightM == (valid ? *sample : 0.0) && profile->Grade == 0,
                "profile preserves signed finite heights and substitutes missing values");
        }
      }
      const auto empty = network.Elevate({});
      CHECK(empty.Points == 0 && empty.Refused == 2, "empty provider refuses all samples");
    }
  }
  return Report();
}
