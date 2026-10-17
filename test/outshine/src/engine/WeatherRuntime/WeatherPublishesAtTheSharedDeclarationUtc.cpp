#include <Outshine.h>
#include <generation/weather/OpenMeteoProvider.h>
#include <world/weather/WeatherSnapshot.h>
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
  auto query =
      (*sources)->Sources().Ask(Data::Fetch(Data::DataKind::Weather, Data::Address::Whole(0)));
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
              .setRoots({.Assets = {},
                         .Shipped = {},
                         .Cache = directory,
                         .Offline = true,
                         .AssetCache = {}})
              .has_value(),
          "the engine uses the prepared source cache and disallows network acquisition");
    Scenario::Document scenario;
    scenario.Providers.push_back(capture->Declaration);
    scenario.Time = {.Declared = true, .Start = "2026-10-15T00:00:00Z"};
    scenario.Lit.Declared = true;
    constexpr double kManualSunElevationDeg = 23.0;
    scenario.Lit.Key.ElevationDeg = kManualSunElevationDeg;
    constexpr double kLegacyLowCover = 0.65;
    scenario.Ground.Sky.CloudLow = kLegacyLowCover;
    scenario.Ground.Sky.CloudHigh = 1.0;
    auto metadataOnly = scenario;
    metadataOnly.Providers.clear();
    CHECK(engine.declare(metadataOnly) && engine.assemble() && engine.preload(0.0),
          "legacy sky metadata alone requires no source or terrain acquisition");
    const auto absent = engine.weather();
    CHECK(absent && !*absent && engine.loading().ProviderStarts == 0,
          "legacy scalar sky fields do not manufacture a native weather publication");
    CHECK(engine.declare(scenario).has_value(), "manual light and headless weather retain UTC");
    auto invalid = scenario;
    invalid.Time.Start = "not-an-ISO-UTC";
    CHECK(!engine.declare(invalid) && engine.declaration().Time.Start == scenario.Time.Start,
          "headless manual light cannot bypass invalid UTC or replace the valid declaration");
    CHECK(!engine.weather(), "an unassembled declaration cannot expose the previous weather");
    CHECK(engine.assemble().has_value(), "no renderer or terrain is required to prepare weather");
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
    CHECK(engine.assemble() && engine.preload(0.0) && engine.weather() &&
              *engine.weather() == retained && engine.loading().SourceDeliveries == 1,
          "an unchanged request reuses its exact publication without fetching or normalizing");
    scenario.Time.Start = "2026-10-15T00:00:01Z";
    CHECK(engine.declare(scenario) && !engine.weather() && engine.assemble() &&
              engine.preload(kPreloadTimeoutS),
          "another declared UTC invalidates the former publication before selection");
    CHECK(engine.weather() && !*engine.weather() && engine.settled(),
          "an absent exact time is terminal missing, never nearest-hour or declared fallback");
    scenario.Providers.clear();
    CHECK(engine.declare(scenario) && engine.assemble() && engine.preload(0.0),
          "removing the provider releases the source scope without stale weather");
    CHECK(engine.weather() && !*engine.weather(),
          "no requested weather remains an empty selection");
  }
  CHECK(retained && retained->Source.PayloadSha256 == capture->Declaration.PayloadSha256 &&
            retained->Values.ValidUtcS == kCapturedWeatherUtcS,
        "the returned immutable native snapshot survives redeclaration and engine destruction");
  std::filesystem::remove_all(directory);
  return Report();
}
