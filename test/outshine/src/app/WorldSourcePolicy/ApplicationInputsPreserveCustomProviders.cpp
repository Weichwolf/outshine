#include "Check.h"
#include "src/app/WorldSourcePolicy.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document world;
  world.Ground.Declared = true;
  CHECK(!App::ValidateWorldSources(world), "real client worlds require a geographic vector source");
  CHECK(App::ConfigureWorldSources(world) && world.Providers.size() == 3 &&
            world.Providers.front().Kind == "vector" &&
            world.Providers.front().Schema == "openmaptiles" &&
            world.Providers.front().Endpoint.starts_with("https://tiles.openfreemap.org/planet/") &&
            !world.Providers.front().Coverage && world.Providers.front().Location.empty() &&
            Data::ValidateSourceProviders(world.Providers),
        "unconfigured client worlds declare the pinned preferred OpenMapTiles dataset");
  const auto declared = world.Providers;
  CHECK(App::ConfigureWorldSources(world) && world.Providers == declared,
        "explicit source declaration is retained without another catalogue");
  world.Providers.clear();
  Data::SourceProvider provider;
  provider.Kind = "osm";
  world.Providers.push_back(provider);
  CHECK(App::ValidateWorldSources(world), "original OSM selects the client world path");
  provider.Kind = "vector";
  world.Providers.push_back(provider);
  CHECK(App::ValidateWorldSources(world),
        "client worlds accept explicitly declared vector sources");
  world.Providers.clear();
  world.Ground.Shape.Kind = "plane";
  CHECK(App::ValidateWorldSources(world), "explicit analytic diagnoses remain available");
  CHECK(App::ConfigureWorldSources(world) && world.Providers.empty(),
        "analytic diagnoses never acquire geographic inputs");
  return Report();
}
