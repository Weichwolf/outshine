#include "ScenarioInput.h"
#include "Engine.h"
#include "Generator.h"
#include "Check.h"
#include <string_view>

namespace {
class Probe final : public outshine::Generators::Generator {
public:
  mutable unsigned Calls = 0;

  std::string_view kind() const override { return "scenario-probe"; }

  Product make([[maybe_unused]] const outshine::Generators::Request &request) const override {
    ++Calls;
    return outshine::Geometry{};
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Probe probe;
  const auto loaded =
      App::ReadScenarioInput(R"({"generators":{"generator":[{"kind":"scenario-probe"}]}})", {}, {});
  CHECK(loaded, "client decoding accepts a generator that belongs to the actual engine");
  CHECK(probe.Calls == 0, "loading performs no generator execution");
  if (!loaded) { return Report(); }
  Engine engine;
  CHECK(engine.registerGenerator(probe), "actual engine owns generator registration");
  CHECK(engine.declare(*loaded), "actual engine declares the decoded scenario once");
  CHECK(probe.Calls == 1, "generator executes only during the explicit declaration");
  const auto previous = engine.writeScenario();
  CHECK(!App::ReadScenarioInput("{", {}, {}), "malformed replacement is refused");
  CHECK(probe.Calls == 1 && engine.writeScenario() == previous,
        "failed decoding neither executes content creation nor changes the active engine");
  return Report();
}
