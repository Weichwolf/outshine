#include "Check.h"
#include "ScenarioRead.h"
#include "ScenarioWrite.h"

#include <scenario/Scenario.h>

#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  constexpr std::string_view xml =
      "<scenario><providers><provider kind='terrain' dataset='dem.example' "
      "endpoint='https://dem.example/tiles/{z}/{x}/{y}.png' pin='2026-09' "
      "rank='-2' whenAbsent='fail'/></providers></scenario>";
  Scenario::Document scenario;
  std::string error;
  CHECK(ReadScenario(xml.data(), xml.size(), scenario, error), error.c_str());
  CHECK(scenario.Providers.size() == 1 &&
            scenario.Providers.front().Endpoint == "https://dem.example/tiles/{z}/{x}/{y}.png" &&
            scenario.Providers.front().Dataset == "dem.example",
        "parser retains source identity and endpoint");
  const auto written = WriteScenario(scenario);
  CHECK(written.has_value(), "writer accepts declared tile endpoint");
  if (written) {
    Scenario::Document roundTrip;
    error.clear();
    CHECK(ReadScenario(written->data(), written->size(), roundTrip, error) &&
              roundTrip.Providers == scenario.Providers,
          "tile endpoint survives scenario roundtrip");
  }
  for (const std::string_view rejected :
       {"<scenario><providers><provider kind='terrain' endpoint='http://a/{z}/{x}/{y}' "
        "dataset='a'/></providers></scenario>",
        "<scenario><providers><provider kind='terrain' endpoint='https://a/{z}/{x}/{y}'/>"
        "</providers></scenario>",
        "<scenario><providers><provider kind='osm' endpoint='https://a/{z}/{x}/{y}'/>"
        "</providers></scenario>"}) {
    Scenario::Document unchanged = scenario;
    error.clear();
    CHECK(!ReadScenario(rejected.data(), rejected.size(), unchanged, error) &&
              unchanged.Providers == scenario.Providers,
          "invalid endpoint fails without replacing prior declaration");
  }
  return Report();
}
