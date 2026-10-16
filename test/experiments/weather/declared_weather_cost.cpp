#include <Outshine.h>
#include "DeclaredWeather.h"
#include "DeclaredWeatherCapture.h"
#include "ScenarioRead.h"
#include "Check.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Clock = std::chrono::steady_clock;

template <typename Operation> void Measure(const char *name, Operation operation) {
  constexpr size_t kIterations = 1000;
  const auto begin = Clock::now();
  for (size_t at = 0; at < kIterations; ++at) { operation(); }
  const auto ns = std::chrono::duration<double, std::nano>(Clock::now() - begin).count();
  std::printf("%s iterations=%zu mean_ns=%.3f\n", name, kIterations, ns / kIterations);
}

void EngineCase(Scenario::Document scenario, const char *name) {
  Engine engine;
  CHECK(engine.setRoots(
            {.Assets = {}, .Shipped = {}, .Cache = {}, .Offline = true, .AssetCache = {}}),
        "the measured headless engine uses no source cache or network");
  Measure(name, [&] {
    if (!engine.declare(scenario) || !engine.assemble() || !engine.preload(0.0)) { std::abort(); }
  });
  const auto retained = engine.weather();
  const auto repeatedName = std::string(name) + "-repeat";
  Measure(repeatedName.c_str(), [&] {
    if (!engine.assemble() || !engine.preload(0.0) || engine.weather() != retained) {
      std::abort();
    }
  });
  const auto loading = engine.loading();
  CHECK(loading.ProviderStarts == 0 && loading.SourceDeliveries == 0 && loading.Sources.empty(),
        "all measured cases remain free of provider acquisition");
}
}

int main() {
  const auto input = DeclaredWeatherScenario();
  CHECK(input, "the same fixed native declaration is available for cost measurement");
  if (!input) { return Report(); }
  Scenario::Document scenario;
  std::string error;
  CHECK(ReadScenario(input->data(), input->size(), scenario, error) && scenario.Weather,
        "the measured native input passes production parsing");
  if (!scenario.Weather) { return Report(); }
  std::printf("WeatherValues_bytes=%zu WeatherSnapshot_bytes=%zu WeatherDeclaration_bytes=%zu "
              "optional_declaration_bytes=%zu\n",
              sizeof(WeatherValues),
              sizeof(WeatherSnapshot),
              sizeof(Scenario::WeatherDeclaration),
              sizeof(std::optional<Scenario::WeatherDeclaration>));
  Measure("native-input", [&] {
    const auto result = Generators::Weather::MakeDeclaredWeather(
        scenario.Weather->At, scenario.Weather->Values, kDeclaredWeatherUtcS);
    if (!result || result->Values.CloudLow != kDeclaredWeatherLow) { std::abort(); }
  });
  Measure("owned-snapshot", [&] {
    auto result = Generators::Weather::MakeDeclaredWeather(
        scenario.Weather->At, scenario.Weather->Values, kDeclaredWeatherUtcS);
    if (!result) { std::abort(); }
    const auto owned = std::make_shared<const WeatherSnapshot>(std::move(*result));
    if (owned->Values.CloudLow != kDeclaredWeatherLow) { std::abort(); }
  });
  EngineCase(scenario, "declared-new");
  scenario.Providers = {{.Kind = "terrain"}};
  EngineCase(scenario, "declared-terrain-new");
  scenario.Weather.reset();
  EngineCase(scenario, "terrain-new");
  scenario.Providers.clear();
  EngineCase(std::move(scenario), "empty-new");
  return Report();
}
