#include <Outshine.h>
#include <generation/Generate.h>
#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Structures.h"
#include "Check.h"
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace {
class Probe final : public outshine::Generators::Generator {
public:
  mutable std::vector<std::pair<std::string, std::string>> Seen;
  mutable int Calls = 0;

  std::string_view kind() const override { return "parameter-probe"; }

  Product make(const outshine::Generators::Request &request) const override {
    ++Calls;
    Seen.clear();
    for (const auto &parameter : request.Parameters) {
      Seen.emplace_back(parameter.Name, parameter.Value);
    }
    return outshine::Geometry{};
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Probe probe;
  Engine engine;
  CHECK(engine.offers(probe), "fixture generator registers");
  Scenario::Document source;
  source.Generators.push_back(
      {.Kind = "parameter-probe",
       .Parameters = {{.Name = "label", .Value = "A & <B> \"quoted\"\nnext"},
                      {.Name = "scale", .Value = "1.2500"}}});
  const auto xml = WriteScenario(source);
  CHECK(xml.has_value(), "scenario export succeeds");
  if (!xml) { return Report(); }
  Scenario::Document copy;
  std::string error;
  CHECK(ReadScenario(xml->data(), xml->size(), copy, error), "written generator settings parse");
  CHECK(engine.declare(copy).has_value(), "registered generator succeeds through public API");
  const std::vector<std::pair<std::string, std::string>> expected{
      {"label", "A & <B> \"quoted\"\nnext"}, {"scale", "1.2500"}};
  CHECK(probe.Calls == 1 && probe.Seen == expected,
        "serialization and dispatch preserve exact values and order");
  Scenario::Document asset;
  asset.Assets.push_back({.Uri = "parameter-probe", .Kind = "generated"});
  CHECK(engine.declare(asset).has_value() && probe.Calls == 2 && probe.Seen.empty(),
        "asset invocation cannot inherit another declaration's parameter views");
  copy.Generators = {{.Kind = "unregistered"}};
  CHECK(!engine.declare(copy), "unknown generator is not silently skipped");
  Generators::Structures structures;
  const std::array<Generators::Parameter, 1> unsupported{{{.Name = "unknown", .Value = "1"}}};
  Generators::Request request;
  request.Parameters = unsupported;
  CHECK(!structures.make(request), "built-in producer refuses unsupported settings");
  return Report();
}
