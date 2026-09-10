#include "ScenarioRead.h"
#include "Check.h"
#include <limits>
#include <string>
#include <string_view>

namespace {
std::string Scene(std::string_view attributes) {
  return "<scenario><world><osm><way kind=\"track\" points=\"0,0 1,1\"/>"
         "<way kind=\"residential\" points=\"2,2 3,3\" " +
         std::string(attributes) + "/></osm></world></scenario>";
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const auto name : {"widthM", "heightM", "level"}) {
    for (const auto value : {"", "x", "1x", "nan", "inf", "1e999", " 1", "1 "}) {
      Scenario::Document held;
      held.Ground.Osm.push_back({.Kind = "retained"});
      const auto xml = Scene(std::string(name) + "=\"" + value + "\"");
      std::string error;
      CHECK(!ReadScenario(xml.data(), xml.size(), held, error) && !error.empty(),
            "invalid explicit number causes a diagnostic");
      CHECK(held.Ground.Osm.size() == 1 && held.Ground.Osm.front().Kind == "retained",
            "invalid late attributes preserve the old document");
    }
  }
  for (const auto attributes : {"widthM=\"-1\"",
                                "heightM=\"-0.01\"",
                                "level=\"1.5\"",
                                "level=\"-1.5\"",
                                "level=\"2147483648\"",
                                "level=\"-2147483649\""}) {
    const auto xml = Scene(attributes);
    Scenario::Document held;
    std::string error;
    CHECK(!ReadScenario(xml.data(), xml.size(), held, error),
          "negative measures and invalid integer levels refused");
  }
  static_assert(std::numeric_limits<int>::digits == 31,
                "level boundary fixtures target 32-bit signed int");
  for (const int level :
       {std::numeric_limits<int>::min(), -1, 0, 1, std::numeric_limits<int>::max()}) {
    const auto xml =
        Scene("widthM=\"3.125e0\" heightM=\"0\" level=\"" + std::to_string(level) + "\"");
    Scenario::Document held;
    std::string error;
    const bool read = ReadScenario(xml.data(), xml.size(), held, error);
    CHECK(read && held.Ground.Osm.size() == 2, "representable levels and dimensions accepted");
    if (held.Ground.Osm.size() != 2) { continue; }
    const auto &feature = held.Ground.Osm.back();
    CHECK(feature.Level == level && feature.WidthM == 3.125 && feature.HeightM == 0,
          "attributes retain their exact declared values");
    const auto &defaults = held.Ground.Osm.front();
    CHECK(defaults.WidthM == 0 && defaults.HeightM == 0 && defaults.Level == 0,
          "missing attributes select documented zero defaults");
  }
  return Report();
}
