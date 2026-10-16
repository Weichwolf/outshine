#include "ScenarioLayer.h"
#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "DeclaredWeatherCapture.h"
#include "Check.h"

#include <string>
#include <string_view>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto input = DeclaredWeatherScenario();
  CHECK(input.has_value(), "the original declared-weather input is available");
  if (!input) { return Report(); }
  Scenario::Document scenario;
  std::string error;
  CHECK(ReadScenario(input->data(), input->size(), scenario, error),
        "the base declaration has an explicit clock and native weather");
  const auto original = WriteScenario(scenario);
  CHECK(original.has_value(), "the original declaration is serializable");
  std::vector<std::string> trace{"before"};
  for (
      const std::string_view invalid :
      {R"(<scenario><clock start="2026-10-15T01:00:00Z"/><events><event name="new"/></events></scenario>)",
       R"(<scenario><weather validUtcS="1792022401" latitudeDeg="0" longitudeDeg="0" cloudLow="0"/></scenario>)",
       R"(<scenario><weather validUtcS="1792022400" latitudeDeg="0" longitudeDeg="0" cloudLow="2"/></scenario>)"}) {
    CHECK(!ApplyLayer(scenario, invalid.data(), invalid.size(), "invalid", trace, error),
          "a bad native input or combined clock mismatch rejects the whole layer");
    CHECK(!error.empty() && WriteScenario(scenario) == original &&
              trace == std::vector<std::string>{"before"},
          "rejection preserves every document field and the existing layer trace");
  }
  constexpr std::string_view unrelated =
      R"(<scenario><events><event name="unrelated"/></events></scenario>)";
  CHECK(ApplyLayer(scenario, unrelated.data(), unrelated.size(), "event", trace, error) &&
            scenario.Weather && scenario.Weather->Values.CloudLow == kDeclaredWeatherLow,
        "an unrelated layer preserves the complete explicit weather input");
  constexpr std::string_view clear =
      R"(<scenario><weather validUtcS="1792022400" latitudeDeg="0" longitudeDeg="0" cloudLow="0"/></scenario>)";
  CHECK(ApplyLayer(scenario, clear.data(), clear.size(), "clear", trace, error),
        "a valid replacement passes the combined clock check");
  CHECK(error.empty() && scenario.Weather && scenario.Weather->Values.CloudLow == 0.0 &&
            !scenario.Weather->Values.CloudHigh && !scenario.Weather->Values.Wind10m &&
            !scenario.Weather->Values.Precipitation.TotalWaterKgM2 &&
            !scenario.Weather->At.DeclaredSurfaceElevationAslM &&
            scenario.Weather->At.LatitudeDeg == 0.0 && scenario.Events.size() == 1,
        "weather replacement is whole: omitted values remain unknown and other sections survive");
  return Report();
}
