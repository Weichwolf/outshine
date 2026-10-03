#include "Check.h"
#include "src/client/WorldSourcePolicy.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document world;
  world.Ground.Declared = true;
  CHECK(!Client::ValidateWorldSources(world),
        "real client worlds require a geographic vector source");
  CHECK(Client::ConfigureWorldSources(world) && world.Providers.size() == 3 &&
            world.Providers.front().Kind == "vector" &&
            world.Providers.front().Schema == "openmaptiles" &&
            world.Providers.front().Endpoint.starts_with("https://tiles.openfreemap.org/planet/") &&
            !world.Providers.front().Coverage && world.Providers.front().Location.empty() &&
            Data::ValidateSourceProviders(world.Providers),
        "unconfigured client worlds declare the pinned preferred OpenMapTiles dataset");
  const auto declared = world.Providers;
  CHECK(Client::ConfigureWorldSources(world) && world.Providers == declared,
        "explicit source declaration is retained without another catalogue");
  world.Providers.clear();
  world.Providers.push_back({.Kind = "osm"});
  CHECK(Client::ValidateWorldSources(world), "original OSM selects the client world path");
  world.Providers.push_back({.Kind = "vector"});
  CHECK(Client::ValidateWorldSources(world),
        "client worlds accept explicitly declared vector sources");
  world.Providers.clear();
  world.Ground.Shape.Kind = "plane";
  CHECK(Client::ValidateWorldSources(world), "explicit analytic diagnoses remain available");
  CHECK(Client::ConfigureWorldSources(world) && world.Providers.empty(),
        "analytic diagnoses never acquire geographic inputs");
  return Report();
}
