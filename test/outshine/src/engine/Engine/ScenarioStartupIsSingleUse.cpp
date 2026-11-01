#include "Engine.h"
#include "Generator.h"
#include "Check.h"
#include <string_view>

namespace {
class Probe final : public outshine::Generators::Generator {
public:
  mutable unsigned Calls = 0;

  std::string_view kind() const override { return "startup-probe"; }

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
  Engine engine;
  CHECK(engine.registerGenerator(probe), "startup generator registers");
  Scenario::Document original;
  original.Room = 2;
  original.Motion.StepS = 0.02;
  original.Generators.push_back({.Kind = "startup-probe", .Parameters = {}});
  CHECK(engine.loadScenario(original), "the initial scenario starts");
  CHECK(probe.Calls == 1, "startup generates content once");
  auto &entities = engine.entities();
  const auto marker = entities.addEntity();
  CHECK(marker, "the started registry accepts an entity");
  const auto saved = engine.writeScenario();
  CHECK(saved, "the started scenario exports");
  auto different = original;
  different.Motion.StepS = 0.04;
  for (const auto *scenario : {&original, &different}) {
    const auto repeated = engine.loadScenario(*scenario);
    CHECK(!repeated && repeated.error().contains("configured"),
          "a configured engine rejects another startup before preparing content");
    CHECK(probe.Calls == 1 && engine.writeScenario() == saved,
          "rejected startup performs no generation or configuration changes");
    CHECK(&engine.entities() == &entities && marker && entities.alive(*marker),
          "rejected startup preserves the live registry");
    CHECK(engine.stepSeconds() == original.Motion.StepS,
          "rejected startup preserves the live simulation step");
  }
  return Report();
}
