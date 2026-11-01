#include <Engine.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Engine engine;
  const Scenario::Document original;
  CHECK(engine.loadScenario(original), "empty scenario declaration is valid");

  Scenario::Document invalid = original;
  invalid.Routes.push_back({.Id = "grand-prix", .OsmRelationId = 284588});
  CHECK(!engine.loadScenario(invalid) && engine.declaration().Routes.empty(),
        "a named source route cannot publish without a semantic OSM provider");

  invalid.Routes.push_back(invalid.Routes.front());
  CHECK(!engine.loadScenario(invalid) && engine.declaration().Routes.empty(),
        "duplicate programmatic route names preserve the previous declaration");
  return Report();
}
