#include "EngineFactory.h"
#include <Engine.h>
#include "Check.h"

#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const Scenario::Document original;
  const auto load = [](const Scenario::Document &scenario) {
    auto fresh = App::CreateEngine();
    return fresh.loadScenario(scenario);
  };
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
  CHECK(!load(candidate), "programmatic declaration rejects nonfinite source bounds");

  coverage.NorthDeg = 49.34;
  candidate.Providers.front().Coverage = coverage;
  candidate.Providers.front().Location = "https://example.invalid/region.osm";
  CHECK(!load(candidate), "programmatic declaration rejects URL disguised as local file");

  candidate.Providers.front().Location = "osm/region.osm";
  candidate.Providers.front().Revision = "sha256:short";
  CHECK(!load(candidate), "malformed content pin rejects the whole declaration");

  candidate.Providers.front().Revision = "pin-r1";
  auto engine = App::CreateEngine();
  CHECK(engine.loadScenario(candidate).has_value(),
        "valid source declaration starts independently");
  const auto written = engine.writeScenario();
  CHECK(written && written->contains("dataset=\"openstreetmap\""),
        "public scenario writer retains source identity");
  return Report();
}
