#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "ScenarioJson.h"
#include "DeclaredWeatherCapture.h"
#include "Check.h"

#include <string>
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scenario;
  std::string error;
  CHECK(ReadScenario(
            kDeclaredWeatherScenario.data(), kDeclaredWeatherScenario.size(), scenario, error),
        "native weather XML is a declared scenario section without terrain or providers");
  if (!scenario.Weather) { return Report(); }
  const auto &weather = *scenario.Weather;
  CHECK(weather.Values.ValidUtcS == kDeclaredWeatherUtcS &&
            weather.Values.CloudLow == kDeclaredWeatherLow && weather.Values.CloudMid == 0.0 &&
            weather.Values.SnowDepthM == 0.0 && !scenario.Ground.Declared,
        "native UTC, known-zero fields and the inactive terrain section are retained");
  constexpr double kEastMs = 8.0;
  constexpr double kRainKgM2 = 2.0;
  constexpr int64_t kHourS = 3600;
  CHECK(weather.Values.Wind10m && weather.Values.Wind10m->EastMs == kEastMs &&
            weather.Values.Wind10m->NorthMs == 0.0 &&
            weather.Values.Precipitation.BeginUtcS == kDeclaredWeatherUtcS - kHourS &&
            weather.Values.Precipitation.EndUtcS == kDeclaredWeatherUtcS &&
            weather.Values.Precipitation.RainWaterKgM2 == kRainKgM2 &&
            weather.Values.Precipitation.SnowfallDepthM == 0.0,
        "ENU wind and preceding amounts retain their named native units");
  const auto xml = WriteScenario(scenario);
  CHECK(xml.has_value(), "native values are exported through the existing scenario writer");
  if (!xml) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(xml->data(), xml->size(), copy, error), "exported native weather reads again");
  const auto repeated = WriteScenario(copy);
  CHECK(repeated && *repeated == *xml, "every exported field and interval round trips exactly");
  const auto patch = ScenarioXmlFromJson(
      R"({"weather":{"cloudLow":null,"wind10m":null,"precipitation":{"rainWaterKgM2":null}}})",
      *xml);
  CHECK(patch && ReadScenario(patch->data(), patch->size(), copy, error),
        "JSON null removes individual optional native inputs");
  CHECK(copy.Weather && !copy.Weather->Values.CloudLow && !copy.Weather->Values.Wind10m &&
            !copy.Weather->Values.Precipitation.RainWaterKgM2 &&
            copy.Weather->Values.SnowDepthM == 0.0 &&
            copy.Weather->Values.Precipitation.EndUtcS == kDeclaredWeatherUtcS,
        "removal means missing while unrelated known zeros and the interval remain");
  const auto none = ScenarioXmlFromJson(R"({"weather":null})", *xml);
  CHECK(none && ReadScenario(none->data(), none->size(), copy, error) && !copy.Weather,
        "removing the native declaration does not create weather from legacy defaults");
  for (const auto *bad : {R"({"weather":{"cloudLow":2}})",
                          R"({"weather":{"validUtcS":"1792022400junk"}})",
                          R"({"weather":{"validUtcS":1792022401}})",
                          R"({"weather":{"wind10m":{"eastMs":null}}})",
                          R"({"weather":{"precipitation":{"endUtcS":1792022401}}})"}) {
    const auto invalid = ScenarioXmlFromJson(bad, *xml);
    CHECK(invalid && !ReadScenario(invalid->data(), invalid->size(), copy, error) && !copy.Weather,
          "bad values, integer tokens, UTC, vector structure and intervals fail atomically");
  }
  return Report();
}
