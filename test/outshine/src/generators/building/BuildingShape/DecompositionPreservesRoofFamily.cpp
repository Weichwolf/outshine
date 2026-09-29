#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  BuildingScratch scratch;
  // OSM way 87700216: an irregular church outline, not a round building.
  constexpr std::array<double, 34> outline{
      54.7831088, 9.4360635,  54.7831535, 9.4361430,  54.7832826, 9.4360994,  54.7833197,
      9.4360087,  54.7832902, 9.4357389,  54.7833433, 9.4357196,  54.7833255, 9.4355566,
      54.7832720, 9.4355743,  54.7832428, 9.4353071,  54.7831824, 9.4353270,  54.7831779,
      9.4352799,  54.7831643, 9.4351681,  54.7829668, 9.4352352,  54.7830074, 9.4356083,
      54.7830332, 9.4355998,  54.7830454, 9.4357114,  54.7830670, 9.4357043};
  for (const double pitched : {-1.0, 1.0}) {
    const auto parts = MassOf(
        outline, {.HeightM = 40.0, .HeightMeasured = true, .PitchedShare = pitched}, {}, scratch);
    CHECK(parts && parts->size() > 1,
          "the irregular footprint exercises actual wing and setback decomposition");
    CHECK(parts &&
              std::ranges::none_of(
                  *parts, [](const BuildingShape &part) { return part.Roof == RoofKind::Dome; }),
          "a split part cannot invent a dome absent from the whole building design");
  }
  std::array<double, 32> round{};
  for (size_t i = 0; i < round.size() / 2; ++i) {
    const double angle = 2.0 * std::numbers::pi * static_cast<double>(i) / 16.0;
    round[2 * i] = 49.0 + 20.0 * std::sin(angle) / 111320.0;
    round[2 * i + 1] =
        8.0 + 20.0 * std::cos(angle) / (111320.0 * std::cos(49.0 * std::numbers::pi / 180.0));
  }
  const auto dome = MassOf(round, {.HeightM = 40.0}, {}, scratch);
  CHECK(dome && !dome->empty() &&
            std::ranges::all_of(
                *dome, [](const BuildingShape &part) { return part.Roof == RoofKind::Dome; }),
        "a genuinely round whole building retains its domed design");
  return Report();
}
