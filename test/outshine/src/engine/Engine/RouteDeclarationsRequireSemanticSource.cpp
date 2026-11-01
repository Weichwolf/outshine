#include "ScenarioStartup.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const Scenario::Document original;
  CHECK(LoadFreshScenario(original), "empty scenario declaration is valid");

  Scenario::Document invalid = original;
  invalid.Routes.push_back({.Id = "grand-prix", .OsmRelationId = 284588});
  CHECK(!LoadFreshScenario(invalid),
        "a named source route cannot publish without a semantic OSM provider");

  invalid.Routes.push_back(invalid.Routes.front());
  CHECK(!LoadFreshScenario(invalid), "duplicate programmatic route names are rejected");
  return Report();
}
