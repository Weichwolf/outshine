#include "ScenarioLayer.h"
#include "Check.h"

#include <string>
#include <string_view>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Scenario::Document document;
  document.Providers.push_back(
      {.Kind = "osm",
       .Revision = "r1",
       .Missing = Data::MissingDataPolicy::Fail,
       .Dataset = "openstreetmap",
       .Location = "old.osm",
       .Coverage = Data::SourceCoverage{
           .WestDeg = 8.5, .SouthDeg = 49.3, .EastDeg = 8.6, .NorthDeg = 49.4}});
  constexpr std::string_view layer =
      "<scenario><providers>"
      "<provider kind='osm' dataset='openstreetmap' location='west.osm' pin='r1' "
      "rank='0' whenAbsent='fail' westDeg='8.5' southDeg='49.3' "
      "eastDeg='8.6' northDeg='49.4'/>"
      "<provider kind='osm' dataset='openstreetmap' location='east.osm' pin='r1' "
      "rank='1' whenAbsent='fail' westDeg='8.6' southDeg='49.3' "
      "eastDeg='8.7' northDeg='49.4'/>"
      "</providers></scenario>";
  std::vector<std::string> trace;
  std::string error;
  CHECK(ApplyLayer(document, layer.data(), layer.size(), "regions", trace, error),
        "two regional OSM chunks survive layer composition");
  CHECK(document.Providers.size() == 2 && document.Providers[0].Location == "west.osm" &&
            document.Providers[1].Location == "east.osm" && document.Providers[0].Priority == 0 &&
            document.Providers[1].Priority == 1,
        "same-rank chunk overrides while distinct-rank chunk is appended");
  CHECK(trace.size() == 2 && trace[0].find("overrode") != std::string::npos &&
            trace[1].find("added") != std::string::npos,
        "layer diagnostics identify replacement and addition separately");
  return Report();
}
