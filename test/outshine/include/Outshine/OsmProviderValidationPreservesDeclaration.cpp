#include <Outshine.h>
#include "Check.h"

#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Engine engine;
  Scenario::Document original;
  original.Named.Name = "kept";
  CHECK(engine.declare(original).has_value(), "initial native declaration accepted");
  const auto before = engine.writeScenario();

  auto candidate = original;
  candidate.Providers.push_back(
      {.Kind = "osm",
       .Revision = "pin-r1",
       .Missing = Data::MissingDataPolicy::Fail,
       .Dataset = "openstreetmap",
       .Location = "osm/region.osm",
       .Coverage = Data::SourceCoverage{
           .WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34}});
  candidate.Providers.front().Coverage->NorthDeg = std::numeric_limits<double>::infinity();
  CHECK(!engine.declare(candidate), "programmatic declaration rejects nonfinite source bounds");
  CHECK(engine.writeScenario() == before, "invalid source preserves the previous declaration");

  candidate.Providers.front().Coverage->NorthDeg = 49.34;
  candidate.Providers.front().Location = "https://example.invalid/region.osm";
  CHECK(!engine.declare(candidate), "programmatic declaration rejects URL disguised as local file");
  CHECK(engine.writeScenario() == before, "second invalid source also preserves declaration");

  candidate.Providers.front().Location = "osm/region.osm";
  candidate.Providers.front().Revision = "sha256:short";
  CHECK(!engine.declare(candidate), "malformed content pin rejects the whole declaration");
  CHECK(engine.writeScenario() == before, "malformed pin preserves the previous declaration");

  candidate.Providers.front().Revision = "pin-r1";
  CHECK(engine.declare(candidate).has_value(), "valid source declaration can replace refusal");
  const auto written = engine.writeScenario();
  CHECK(written && written->find("dataset=\"openstreetmap\"") != std::string::npos,
        "public scenario writer retains source identity");
  return Report();
}
