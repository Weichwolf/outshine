#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::string error;
  for (bool enabled : {false, true}) {
    Scenario::Document declared;
    declared.Ground.Declared = true;
    declared.Ground.VegetationEnabled = enabled;
    const auto xml = WriteScenario(declared);
    CHECK(xml.has_value(), "vegetation configuration exports");
    if (!xml) { continue; }
    Scenario::Document restored;
    CHECK(ReadScenario(xml->data(), xml->size(), restored, error),
          "vegetation configuration imports");
    CHECK(restored.Ground.VegetationEnabled == enabled,
          "both vegetation states survive serialization");
  }
  constexpr std::string_view old = "<scenario><world/></scenario>";
  Scenario::Document defaults;
  CHECK(ReadScenario(old.data(), old.size(), defaults, error) && defaults.Ground.VegetationEnabled,
        "legacy scenarios enable vegetation by default");
  return Report();
}
