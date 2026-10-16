#include <Outshine.h>
#include <world/weather/WeatherSnapshot.h>
#include "ScenarioRead.h"
#include "DeclaredWeatherCapture.h"
#include "WeatherCapture.h"
#include "LowCloud.h"
#include "Check.h"

#include <filesystem>
#include <memory>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto directory = PlantedPath("engine-declared-weather");
  std::shared_ptr<const WeatherSnapshot> retained;
  {
    Scenario::Document scenario;
    std::string error;
    CHECK(ReadScenario(
              kDeclaredWeatherScenario.data(), kDeclaredWeatherScenario.size(), scenario, error),
          "the public engine test consumes the native XML declaration");
    if (!scenario.Weather) { return Report(); }
    Engine engine;
    CHECK(
        engine.setRoots(
            {.Assets = {}, .Shipped = {}, .Cache = directory, .Offline = true, .AssetCache = {}}) &&
            engine.declare(scenario),
        "native declared weather needs no registered provider or terrain");
    CHECK(!engine.weather(), "weather is not exposed before assembling the current declaration");
    CHECK(engine.assemble() && engine.preload(0.0),
          "the normal public assemble/preload path publishes without waiting for IO");
    const auto selected = engine.weather();
    CHECK(selected && *selected && (**selected).Values.CloudLow == kDeclaredWeatherLow &&
              (**selected).Values.ValidUtcS == kDeclaredWeatherUtcS &&
              (**selected).Source.Origin == WeatherOrigin::ScenarioDeclaration &&
              (**selected).Source.PayloadSha256.empty(),
          "the native publication is owned, exact-time and explicitly declared");
    if (!selected || !*selected) { return Report(); }
    retained = *selected;
    const auto load = engine.loading();
    CHECK(load.ProviderStarts == 0 && load.SourceDeliveries == 0 && load.SourceFromStore == 0 &&
              load.GroundWanted == 0 && load.VectorWanted == 0 && load.Sources.empty() &&
              engine.settled(),
          "declaration readiness requires no fake source delivery, cache hit or world work");
    CHECK(engine.assemble() && engine.preload(0.0) && engine.weather() &&
              *engine.weather() == retained,
          "repeated assembly reuses the same immutable publication");
    const auto recipeRequired = Generators::Weather::MakeLowCloud(*retained);
    CHECK(!recipeRequired &&
              recipeRequired.error() == Generators::Weather::LowCloudFailure::MissingRecipe,
          "the engine publication does not invent cloud geometry or cloud-height advection");
    scenario.Weather->Values.CloudLow = 0.0;
    CHECK(engine.declare(scenario) && !engine.weather() && engine.assemble(),
          "a new declaration invalidates the previous selection until assembly");
    const auto clear = engine.weather();
    CHECK(clear && *clear && (**clear).Values.CloudLow == 0.0 &&
              retained->Values.CloudLow == kDeclaredWeatherLow,
          "known-zero replacement preserves the externally retained positive snapshot");
    if (!clear || !*clear) { return Report(); }
    auto wrongUtc = scenario;
    ++wrongUtc.Weather->Values.ValidUtcS;
    CHECK(!engine.declare(wrongUtc) && engine.weather() && *engine.weather() == *clear,
          "a mismatched UTC cannot replace the valid engine declaration or publication");
    if (const auto capture = RecordedWeatherCapture(); capture) {
      auto ambiguous = scenario;
      ambiguous.Providers.push_back(capture->Declaration);
      const auto result = engine.declare(ambiguous);
      CHECK(!result && result.error().find("mutually exclusive") != std::string::npos &&
                engine.weather() && *engine.weather() == *clear,
            "provider/declaration ambiguity fails before opening or fetching the provider");
    } else {
      CHECK(false, "the original provider fixture is available for the ambiguity control");
    }
    scenario.Weather.reset();
    CHECK(engine.declare(scenario) && engine.assemble() && engine.weather() && !*engine.weather(),
          "removing the declaration releases its selected publication without a fallback");
  }
  CHECK(retained && retained->Values.CloudLow == kDeclaredWeatherLow &&
            retained->Source.Origin == WeatherOrigin::ScenarioDeclaration,
        "retained immutable declared weather outlives engine destruction");
  std::filesystem::remove_all(directory);
  return Report();
}
