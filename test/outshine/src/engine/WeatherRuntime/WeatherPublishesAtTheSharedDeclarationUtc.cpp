#include <Engine.h>
#include <OpenMeteoProvider.h>
#include <WeatherSnapshot.h>
#include "WeatherCapture.h"
#include "SourceSession.h"
#include "LowCloud.h"
#include "Check.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr double kPreloadTimeoutS = 5.0;

bool Prime(const WeatherCapture &capture,
           const Generators::Weather::OpenMeteoProvider &provider,
           const std::string &directory) {
  Data::ProviderRegistry registry;
  if (!registry.registerProvider(provider)) { return false; }
  const std::array declarations{capture.Declaration};
  auto sources = Data::SourceSession::Open({.Directory = directory}, declarations, {}, registry);
  if (!sources) { return false; }
  CapturedWeatherTransport wire(capture);
  auto query = (*sources)->Sources().Ask(
      Data::SourceRequest(Data::DataKind::Weather, Data::Address::Whole(0)));
  return (*sources)->Sources().Collect(query, wire).Take().has_value();
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto capture = RecordedWeatherCapture();
  CHECK(capture.has_value(), "the original model forecast and receipt are unchanged");
  if (!capture) { return Report(); }
  const Generators::Weather::OpenMeteoProvider provider(capture->Request);
  const auto directory = PlantedPath("engine-weather-public");
  CHECK(Prime(*capture, provider, directory), "existing source cache receives the original bytes");
  std::shared_ptr<const WeatherSnapshot> retained;
  {
    Engine engine;
    CHECK(engine.registerProvider(provider).has_value(),
          "the normal public provider API registers");
    CHECK(engine
              .setRoots({.BuiltinDataDirectory = {},
                         .SourceCacheDirectory = directory,
                         .Offline = true,
                         .AssetCacheDirectory = {}})
              .has_value(),
          "the engine uses the prepared source cache and disallows network acquisition");
    Scenario::Document scenario;
    scenario.Providers.push_back(capture->Declaration);
    scenario.Time = {.Declared = true, .Start = "2026-10-15T00:00:00Z"};
    scenario.Lit.Declared = true;
    constexpr double kManualSunElevationDeg = 23.0;
    scenario.Lit.Key.ElevationDeg = kManualSunElevationDeg;
    CHECK(engine.loadScenario(scenario).has_value(),
          "manual light and headless weather retain UTC");
    auto invalid = scenario;
    invalid.Time.Start = "not-an-ISO-UTC";
    Engine invalidClock;
    CHECK(!invalidClock.loadScenario(invalid), "headless manual light cannot bypass invalid UTC");

    CHECK(!engine.weather() && !engine.settled(), "unfinished weather is explicit and not ready");
    CHECK(engine.preload(kPreloadTimeoutS).has_value(),
          "normal public preload pumps IO and compute weather work");
    const auto selected = engine.weather();
    CHECK(selected && *selected && (**selected).Values.ValidUtcS == kCapturedWeatherUtcS,
          "the shared resolved declaration UTC selects the exact native weather hour");
    if (!selected || !*selected) { return Report(); }
    retained = *selected;
    CHECK(retained->Values.CloudLow == 0.0 &&
              retained->Source.Origin == WeatherOrigin::ModelForecast,
          "a known-zero model low layer is not replaced by declared cloud or high cover");
    const auto native = Generators::Weather::MakeLowCloud(*retained);
    CHECK(native && *native && (**native).Layer.Cover == 0.0,
          "the public engine publication actually drives the native low-cloud product");
    const auto first = engine.loading();
    CHECK(first.SourceFromStore == 1 && first.ProviderStarts == 0 && first.GroundWanted == 0 &&
              first.VectorWanted == 0 && first.Sources.size() == 1 &&
              first.Sources.front().Kind == "weather",
          "headless loading reports the shared weather source without fetching terrain");
    CHECK(engine.preload(0.0) && engine.weather() && *engine.weather() == retained &&
              engine.loading().SourceDeliveries == 1,
          "an unchanged request reuses its exact publication without fetching or normalizing");
    scenario.Time.Start = "2026-10-15T00:00:01Z";
    Engine missing;
    CHECK(missing.registerProvider(provider), "missing-hour startup registers its provider");
    CHECK(missing.setRoots({.BuiltinDataDirectory = {},
                            .SourceCacheDirectory = directory,
                            .Offline = true,
                            .AssetCacheDirectory = {}}),
          "missing-hour startup shares the offline source cache");
    CHECK(missing.loadScenario(scenario) && !missing.weather() && missing.preload(kPreloadTimeoutS),
          "a different startup time selects its own exact publication");
    CHECK(missing.weather() && !*missing.weather() && missing.settled(),
          "an absent exact time is terminal missing, never nearest-hour fallback");
    scenario.Providers.clear();
    Engine noWeather;
    CHECK(noWeather.loadScenario(scenario) && noWeather.preload(0.0),
          "source-free startup needs no weather acquisition");
    CHECK(noWeather.weather() && !*noWeather.weather(),
          "no requested weather is an empty selection");
  }
  CHECK(retained && retained->Source.PayloadSha256 == capture->Declaration.PayloadSha256 &&
            retained->Values.ValidUtcS == kCapturedWeatherUtcS,
        "the returned immutable native snapshot survives independent startups and engine "
        "destruction");
  std::filesystem::remove_all(directory);
  return Report();
}
