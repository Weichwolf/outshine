#include "ScenarioLoader.h"
#include "Check.h"
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"({"name":"custom",
    "providers":{"provider":[{"kind":"private-domain","dataset":"arbitrary"}]},
    "generators":{"generator":[{"kind":"scenario-probe"}]}})";
  const auto loaded =
      App::LoadScenario({.Source = input, .Overrides = {}, .ResourceDirectory = {}});
  CHECK(loaded, "decoding does not require any registered runtime factories");
  if (!loaded) { return Report(); }
  CHECK(loaded->Document.Providers.size() == 1 &&
            loaded->Document.Providers.front().Kind == "private-domain",
        "provider declarations survive without domain interpretation");
  CHECK(loaded->Document.Generators.size() == 1 &&
            loaded->Document.Generators.front().Kind == "scenario-probe",
        "generator declarations survive without execution");
  CHECK(loaded->LayerTrace.empty(), "layer-free input creates no layer diagnostics");
  CHECK(!App::LoadScenario({.Source = "{", .Overrides = "{}", .ResourceDirectory = {}}),
        "invalid input remains an error without a runtime");
  return Report();
}
