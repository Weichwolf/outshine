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
  bool Accepts = true;

  std::string_view kind() const override { return "redeclare-probe"; }

  bool make(const outshine::Generators::Request &request, outshine::Geometry &) const override {
    ++Calls;
    Value = request.Parameters.empty() ? "" : std::string(request.Parameters.front().Value);
    return Accepts;
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
    engine.offers(probe);
    CHECK(engine.drawsInto(Extent{64, 64}).has_value(), "real offscreen target opens");
    Scenario::Document scenario;
    scenario.Generators.push_back(
        {.Kind = "redeclare-probe", .Parameters = {{.Name = "value", .Value = "first"}}});
    CHECK(engine.declare(scenario) && probe.Calls == 1 && probe.Value == "first",
          "initial declaration invokes producer");
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
  }
  SDL_Quit();
  return Report();
}
