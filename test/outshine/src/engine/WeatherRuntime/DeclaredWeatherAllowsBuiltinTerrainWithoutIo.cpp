#include <Outshine.h>
#include <world/weather/WeatherSnapshot.h>
#include "ScenarioRead.h"
#include "DeclaredWeatherCapture.h"
#include "Check.h"

#include <filesystem>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto input = DeclaredWeatherScenario();
  CHECK(input, "the same original declared-weather input is available");
  if (!input) { return Report(); }
  Scenario::Document scenario;
  std::string error;
  CHECK(ReadScenario(input->data(), input->size(), scenario, error),
        "the declaration starts without world or provider work");
  scenario.Providers.emplace_back();
  scenario.Providers.back().Kind = "terrain";
  const auto directory = PlantedPath("declared-weather-builtin");
  {
    Engine engine;
    CHECK(engine.setRoots(
              {.Assets = {}, .Shipped = {}, .Cache = directory, .Offline = true, .AssetCache = {}}),
          "the headless engine has an isolated offline source root");
    CHECK(engine.declare(scenario),
          "built-in nonweather factories are available during the early category check");
    CHECK(engine.assemble() && engine.preload(0.0),
          "configured terrain can coexist with explicit weather without requesting terrain");
    const auto selected = engine.weather();
    CHECK(selected && *selected && (**selected).Values.CloudLow == kDeclaredWeatherLow &&
              (**selected).Source.Origin == WeatherOrigin::ScenarioDeclaration,
          "the actual nonweather source does not replace the declared weather snapshot");
    const auto loading = engine.loading();
    CHECK(loading.ProviderStarts == 0 && loading.SourceDeliveries == 0 &&
              loading.GroundWanted == 0 && loading.VectorWanted == 0,
          "factory configuration remains separate from provider acquisition and terrain demand");
    scenario.Providers.back().Kind = "unregistered-declared-weather-fixture";
    const auto rejected = engine.declare(scenario);
    CHECK(!rejected && rejected.error().find("unavailable provider") != std::string::npos &&
              engine.weather() == selected,
          "an unavailable factory rejects before replacing the current native publication");
  }
  std::filesystem::remove_all(directory);
  return Report();
}
