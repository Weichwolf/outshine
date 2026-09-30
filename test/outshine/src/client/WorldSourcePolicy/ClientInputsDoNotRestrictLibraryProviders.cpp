#include "Check.h"
#include "src/client/WorldSourcePolicy.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document world;
  world.Ground.Declared = true;
  CHECK(!Client::ValidateWorldSources(world), "real client worlds require original OSM");
  world.Providers.push_back({.Kind = "osm"});
  CHECK(Client::ValidateWorldSources(world), "original OSM selects the client world path");
  world.Providers.push_back({.Kind = "vector"});
  CHECK(!Client::ValidateWorldSources(world),
        "reduced map tiles never replace client OSM originals");
  world.Providers.clear();
  world.Ground.Shape.Kind = "plane";
  CHECK(Client::ValidateWorldSources(world), "explicit analytic diagnoses remain available");
  return Report();
}
