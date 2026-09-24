#include "ScenarioRead.h"
#include "Check.h"

#include <array>
#include <string>
#include <string_view>

namespace {

std::string Changed(std::string text, std::string_view from, std::string_view to) {
  const size_t at = text.find(from);
  if (at != std::string::npos) { text.replace(at, from.size(), to); }
  return text;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const std::string valid =
      "<scenario name='source'><providers>"
      "<provider kind='osm' dataset='openstreetmap' location='region.osm' pin='r1' "
      "rank='0' whenAbsent='fail' westDeg='8.54' southDeg='49.315' "
      "eastDeg='8.61' northDeg='49.34'/>"
      "</providers></scenario>";
  const std::array invalid{
      Changed(valid, "pin='r1' ", ""),
      Changed(valid, "dataset='openstreetmap' ", ""),
      Changed(valid, "location='region.osm' ", ""),
      Changed(valid, "whenAbsent='fail' ", ""),
      Changed(valid, "westDeg='8.54' ", ""),
      Changed(valid, "eastDeg='8.61' ", ""),
      Changed(valid, "eastDeg='8.61'", "eastDeg='nan'"),
      Changed(valid, "westDeg='8.54'", "westDeg='179'"),
      Changed(valid, "location='region.osm'", "location='file://region.osm'"),
      Changed(valid,
              "</providers>",
              "<provider kind='osm' dataset='openstreetmap' location='other.osm' pin='r1' "
              "rank='0' whenAbsent='fail' westDeg='8.54' southDeg='49.315' "
              "eastDeg='8.61' northDeg='49.34'/></providers>"),
      Changed(valid,
              "</providers>",
              "<provider kind='osm' dataset='other' location='other.osm' pin='r1' "
              "rank='1' whenAbsent='fail' westDeg='8.54' southDeg='49.315' "
              "eastDeg='8.61' northDeg='49.34'/></providers>"),
  };

  Scenario::Document document;
  document.Named.Name = "previous";
  document.Providers.push_back({.Kind = "vector", .Revision = "held"});
  std::string error;
  for (const std::string &text : invalid) {
    CHECK(!ReadScenario(text.data(), text.size(), document, error),
          "incomplete, ambiguous or unbounded OSM provider is rejected");
    CHECK(!error.empty() && document.Named.Name == "previous" && document.Providers.size() == 1 &&
              document.Providers.front().Kind == "vector",
          "invalid provider preserves the prior declaration");
  }
  CHECK(ReadScenario(valid.data(), valid.size(), document, error) && error.empty(),
        "valid bounded source declaration succeeds after refusal");
  CHECK(document.Providers.size() == 1 && document.Providers.front().Coverage &&
            document.Providers.front().Coverage->WestDeg == 8.54 &&
            document.Providers.front().Dataset == "openstreetmap" &&
            document.Providers.front().Location == "region.osm",
        "source identity and bounds reach the native declaration");
  return Report();
}
