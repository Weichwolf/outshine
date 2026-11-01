#include "EngineFactory.h"
#include "OsmProviderValidation.h"
#include <Engine.h>
#include "Check.h"

#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  auto engine = App::CreateEngine();
  Scenario::Document original;
  original.Named.Name = "kept";
  CHECK(engine.declare(original) && engine.assemble(), "initial native world assembles");
  const auto *previousSimulation = &engine.entities();
  const auto before = engine.writeScenario();

  auto candidate = original;
  GeographicBounds coverage{.WestDeg = 8.54,
                            .SouthDeg = 49.315,
                            .EastDeg = 8.61,
                            .NorthDeg = std::numeric_limits<double>::infinity()};
  candidate.Providers.push_back({.Kind = "osm",
                                 .Revision = "pin-r1",
                                 .Missing = Data::MissingDataPolicy::Fail,
                                 .Dataset = "openstreetmap",
                                 .Location = "osm/region.osm",
                                 .Endpoint = {},
                                 .Coverage = coverage,
                                 .PayloadSha256 = {},
                                 .Schema = {}});
  CHECK(!engine.declare(candidate), "programmatic declaration rejects nonfinite source bounds");
  CHECK(engine.writeScenario() == before, "invalid source preserves the previous declaration");

  coverage.NorthDeg = 49.34;
  candidate.Providers.front().Coverage = coverage;
  candidate.Providers.front().Location = "https://example.invalid/region.osm";
  CHECK(!engine.declare(candidate), "programmatic declaration rejects URL disguised as local file");
  CHECK(engine.writeScenario() == before, "second invalid source also preserves declaration");

  candidate.Providers.front().Location = "osm/region.osm";
  candidate.Providers.front().Revision = "sha256:short";
  CHECK(!engine.declare(candidate), "malformed content pin rejects the whole declaration");
  CHECK(engine.writeScenario() == before, "malformed pin preserves the previous declaration");

  auto excessive = candidate;
  excessive.Providers.clear();
  for (int rank = 0; rank != 5; ++rank) {
    excessive.Providers.push_back({.Kind = "osm",
                                   .Revision = "pin-r1",
                                   .Priority = rank,
                                   .Missing = Data::MissingDataPolicy::Fail,
                                   .Dataset = "openstreetmap",
                                   .Location = "osm/region.osm",
                                   .Endpoint = {},
                                   .Coverage = candidate.Providers.front().Coverage,
                                   .PayloadSha256 = {},
                                   .Schema = {}});
  }
  CHECK(engine.declare(excessive), "valid source metadata reaches semantic acquisition setup");
  const auto assembled = engine.assemble();
  CHECK(!assembled && assembled.error().contains("four osm chunks"),
        "semantic acquisition rejects a source count outside its supported capacity");
  CHECK(engine.writeScenario() == before && &engine.entities() == previousSimulation &&
            engine.settled(),
        "semantic setup failure preserves the active declaration, simulation and readiness");

  auto catalogue = original;
  catalogue.Providers.push_back(excessive.Providers.front());
  catalogue.Providers.front().Location.clear();
  catalogue.Providers.front().Coverage.reset();
  catalogue.Providers.front().Endpoint = std::string(Generators::Osm::kOfficialOsmApi);
  catalogue.Routes.push_back({.Id = "unsupported", .OsmRelationId = 1});
  CHECK(engine.declare(catalogue), "catalogue source metadata accepts a named route");
  const auto routed = engine.assemble();
  CHECK(!routed && routed.error().contains("catalogue routes require"),
        "missing catalogue route integration is a declared source preparation failure");
  CHECK(engine.writeScenario() == before && &engine.entities() == previousSimulation &&
            engine.settled(),
        "catalogue route setup failure preserves the active world");

  candidate.Providers.front().Revision = "pin-r1";
  CHECK(engine.declare(candidate).has_value(), "valid source declaration can replace refusal");
  const auto written = engine.writeScenario();
  CHECK(written && written->contains("dataset=\"openstreetmap\""),
        "public scenario writer retains source identity");
  return Report();
}
