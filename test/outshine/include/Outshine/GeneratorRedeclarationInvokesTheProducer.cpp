#include <Outshine.h>
#include <SDL3/SDL.h>
#include <generation/Generate.h>
#include "Check.h"
#include <string>
#include <string_view>

namespace {
class Probe final : public outshine::Generators::Generator {
public:
  mutable int Calls = 0;
  mutable std::string Value;
  mutable bool GroundPresent = true;
  bool Accepts = true;

  std::string_view kind() const override { return "redeclare-probe"; }

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
  {
    Probe probe;
    Engine engine;
    CHECK(engine.registerGenerator(probe), "fixture generator registers");
    CHECK(engine.setRenderTarget(Extent{64, 64}).has_value(), "real offscreen target opens");
    Scenario::Document scenario;
    scenario.Generators.push_back(
        {.Kind = "redeclare-probe", .Parameters = {{.Name = "value", .Value = "first"}}});
    CHECK(engine.declare(scenario) && probe.Calls == 1 && probe.Value == "first" &&
              !probe.GroundPresent,
          "groundless declaration invokes producer without inventing a terrain sampler");
    CHECK(engine.declare(scenario) && probe.Calls == 2,
          "unchanged declaration queries producer again; providers may have changed");
    scenario.Generators.front().Parameters.front().Value = "second";
    CHECK(engine.declare(scenario) && probe.Calls == 3 && probe.Value == "second",
          "render reuse cannot swallow changed generator settings");
    probe.Accepts = false;
    CHECK(!engine.declare(scenario) && probe.Calls == 4,
          "a newly refusing producer cannot be reported as successful reuse");
    probe.Accepts = true;
    scenario.Generators.clear();
    scenario.Assets.push_back({.Uri = "redeclare-probe", .Kind = "generated"});
    CHECK(engine.declare(scenario) && probe.Calls == 5 && probe.Value.empty(),
          "generated asset invocation uses its own empty parameters");
    CHECK(engine.declare(scenario) && probe.Calls == 6,
          "generated assets also execute on repeated declarations");
    scenario.Assets.clear();
    CHECK(engine.declare(scenario) && probe.Calls == 6,
          "removing generated content does not invoke removed producer");
    Scenario::Document earth;
    earth.Ground.Declared = true;
    earth.Ground.VegetationEnabled = false;
    earth.Ground.Origin.LatitudeDeg = 49;
    earth.Ground.Origin.LongitudeDeg = 10;
    CHECK(engine.declare(earth), "viewless Earth declares");
    const auto unframed = engine.assemble();
    CHECK(!unframed && unframed.error().contains("ground detail needs"),
          "Earth without a view or geometry rejects invalid ground detail projection");
    Scenario::View earthView;
    earthView.Id = "earth";
    earthView.Person = "first";
    earthView.Placement = Scenario::CameraPlacement::Geodetic;
    earthView.Geographic.Geodetic.LongitudeDeg = 10;
    earthView.Geographic.Geodetic.LatitudeDeg = 49;
    earthView.Geographic.Geodetic.HeightM = 1.7;
    earthView.Geographic.SamplesHeight = false;
    earth.Views.push_back(earthView);
    CHECK(engine.declare(earth) && engine.assemble(),
          "Earth with an explicit view opens after invalid auto framing");
    scenario.Generators.push_back({.Kind = "redeclare-probe"});
    Scenario::View spaceView;
    spaceView.Id = "space";
    spaceView.Person = "first";
    spaceView.Placement = Scenario::CameraPlacement::Local;
    spaceView.Sees.PositionM = {{0, 0, 2}};
    scenario.Views.push_back(spaceView);
    CHECK(engine.declare(scenario) && probe.Calls == 7 && !probe.GroundPresent,
          "a retained Earth stack does not supply ground to a groundless generator");
    CHECK(engine.assemble() && engine.advance(),
          "the replacement assembles and advances without terrain");
    CHECK(!engine.sampleHeight({.LongitudeDeg = 10, .LatitudeDeg = 49}) &&
              engine.loadProgress() == 1.0 && engine.loading().GroundWanted == 0,
          "groundless height and loading APIs ignore retained terrain");
  }
  SDL_Quit();
  return Report();
}
