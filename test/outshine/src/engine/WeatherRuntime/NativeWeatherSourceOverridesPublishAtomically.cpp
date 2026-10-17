#include <Outshine.h>
#include <world/weather/WeatherSnapshot.h>
#include "DeclaredWeatherCapture.h"
#include "Check.h"

#include <filesystem>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view source = "test/outshine/data/weather/declared-overcast.scenario";
  const auto directory = PlantedPath("native-weather-source-overrides");
  {
    Engine engine;
    CHECK(engine.setRoots(
              {.Assets = {}, .Shipped = {}, .Cache = directory, .Offline = true, .AssetCache = {}}),
          "the public source entry uses an isolated offline root");
    CHECK(engine.readScenario(source) && engine.assemble() && engine.preload(0.0),
          "the public file entry publishes native weather through normal readiness");
    const auto initial = engine.weather();
    CHECK(initial && *initial && (**initial).Values.CloudLow == kDeclaredWeatherLow &&
              (**initial).Values.ValidUtcS == kDeclaredWeatherUtcS &&
              (**initial).Source.Origin == WeatherOrigin::ScenarioDeclaration,
          "the published snapshot retains the actual file values and declared origin");
    if (!initial || !*initial) { return Report(); }
    CHECK(engine.readScenario(source, R"({"weather":{"cloudLow":0}})") && engine.assemble(),
          "the public JSON override replaces low cover before publication");
    const auto clear = engine.weather();
    CHECK(clear && *clear && (**clear).Values.CloudLow == 0.0 &&
              (**clear).Values.CloudHigh == 1.0 &&
              (**initial).Values.CloudLow == kDeclaredWeatherLow,
          "known zero replaces the selection while unrelated fields and retained data survive");
    for (const std::string_view invalid : {R"({"weather":{"cloudLow":2}})",
                                           R"({"weather":{"validUtcS":1792022401}})",
                                           R"({"clock":{"start":"2026-10-15T01:00:00Z"}})"}) {
      CHECK(!engine.readScenario(source, invalid) && engine.weather() == clear,
            "invalid public overrides preserve the assembled snapshot atomically");
    }
    CHECK(engine.readScenario(source, R"({"weather":{"cloudLow":null}})") && engine.assemble(),
          "the public override can remove one optional native input");
    const auto unknown = engine.weather();
    CHECK(unknown && *unknown && !(**unknown).Values.CloudLow &&
              (**unknown).Values.CloudHigh == 1.0,
          "an unknown low layer remains a present weather publication");
    CHECK(engine.readScenario(source, R"({"weather":null})") && engine.assemble() &&
              engine.weather() && !*engine.weather(),
          "removing the complete declaration leaves no implicit weather publication");
    constexpr std::string_view json = R"({
      "clock":{"start":"2026-10-15T00:00:00Z"},
      "weather":{"validUtcS":1792022400,"latitudeDeg":47.25,"longitudeDeg":10.25,
      "cloudLow":1,"snowDepthM":0}})";
    CHECK(engine.readScenario(json) && engine.assemble() && engine.preload(0.0),
          "inline JSON reaches the same public native publication without an intermediate API");
    const auto overcast = engine.weather();
    CHECK(overcast && *overcast && (**overcast).Values.CloudLow == 1.0 &&
              (**overcast).Values.SnowDepthM == 0.0 && !(**overcast).Values.CloudHigh,
          "inline JSON preserves known zero and independently missing native fields");
    const auto exported = engine.writeScenario();
    CHECK(exported && exported->find("<weather ") != std::string_view::npos,
          "the public writer retains the owned native declaration");
    const auto loading = engine.loading();
    CHECK(loading.ProviderStarts == 0 && loading.SourceDeliveries == 0 &&
              loading.SourceFromStore == 0 && loading.GroundWanted == 0 &&
              loading.VectorWanted == 0 && loading.Sources.empty(),
          "file and inline input do not become provider or terrain acquisition");
  }
  std::filesystem::remove_all(directory);
  return Report();
}
