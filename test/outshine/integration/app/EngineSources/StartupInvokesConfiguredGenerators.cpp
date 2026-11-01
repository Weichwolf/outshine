#include "EngineFactory.h"
#include "BuiltinSourceProviders.h"
#include <Engine.h>
#include <SDL3/SDL.h>
#include <Generator.h>
#include "Check.h"
#include <expected>
#include <string>
#include <string_view>

namespace {
class Probe final : public outshine::Generators::Generator {
public:
  mutable int Calls = 0;
  mutable std::string Value;
  mutable bool GroundPresent = true;
  bool Accepts = true;

  std::string_view kind() const override { return "startup-probe"; }

  Product make(const outshine::Generators::Request &request) const override {
    ++Calls;
    Value = request.Parameters.empty() ? "" : std::string(request.Parameters.front().Value);
    GroundPresent = request.Ground != nullptr;
    if (!Accepts) { return std::unexpected("refused"); }
    return outshine::Geometry{};
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  for (int variant = 0; variant < 5; ++variant) {
    Probe probe;
    auto engine = App::CreateEngine();
    CHECK(engine.registerGenerator(probe), "startup generator registers");
    CHECK(engine.setRenderTarget(Extent{64, 64}), "startup target opens");
    Scenario::Document scenario;
    if (variant < 3) {
      scenario.Generators.push_back(
          {.Kind = "startup-probe", .Parameters = {{.Name = "value", .Value = "chosen"}}});
    } else if (variant == 3) {
      scenario.Assets.push_back({.Uri = "startup-probe",
                                 .Digest = {},
                                 .Kind = "generated",
                                 .Variant = {},
                                 .Surfaces = {}});
    }
    if (variant == 1) { scenario.Generators.front().Parameters.clear(); }
    probe.Accepts = variant != 2;
    const auto loaded = engine.loadScenario(scenario);
    CHECK(loaded.has_value() == probe.Accepts, "startup propagates generator refusal");
    CHECK(probe.Calls == (variant == 4 ? 0 : 1), "startup invokes only configured content once");
    if (variant != 4) {
      CHECK(!probe.GroundPresent, "groundless startup cannot invent a terrain sampler");
      CHECK(probe.Value == (variant < 3 && variant != 1 ? "chosen" : ""),
            "generator and asset parameters belong to their own invocation");
    }
  }
  {
    auto engine = App::CreateEngine();
    CHECK(engine.setRoots({.BuiltinDataDirectory = "data",
                           .SourceCacheDirectory = {},
                           .Offline = true,
                           .AssetCacheDirectory = {}}),
          "Earth startup stays offline");
    CHECK(engine.setRenderTarget(Extent{64, 64}), "Earth target opens");
    Scenario::Document earth;
    const auto sources = Generators::DefaultSourceProviders();
    earth.Providers.assign(sources.begin(), sources.end());
    earth.Ground.Declared = true;
    earth.Ground.VegetationEnabled = false;
    earth.Ground.Origin.LatitudeDeg = 49;
    earth.Ground.Origin.LongitudeDeg = 10;
    {
      Scenario::View view;
      view.Id = "earth";
      view.Person = "first";
      view.Placement = Scenario::CameraPlacement::Geodetic;
      view.Geographic.Geodetic.LongitudeDeg = 10;
      view.Geographic.Geodetic.LatitudeDeg = 49;
      view.Geographic.Geodetic.HeightM = 1.7;
      view.Geographic.SamplesHeight = false;
      earth.Views.push_back(view);
    }
    const auto loaded = engine.loadScenario(earth);
    CHECK(loaded, loaded ? "explicit Earth camera starts" : loaded.error().c_str());
  }
  SDL_Quit();
  return Report();
}
