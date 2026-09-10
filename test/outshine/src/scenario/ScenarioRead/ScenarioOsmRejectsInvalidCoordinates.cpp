#include "ScenarioRead.h"
#include "Check.h"
#include <string>
#include <string_view>

namespace {
std::string Scene(std::string_view tag, std::string_view points) {
  return "<scenario><world><osm><way kind=\"track\" points=\"0,0 1,1\"/><" + std::string(tag) +
         " kind=\"building\" points=\"" + std::string(points) + "\"/></osm></world></scenario>";
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const auto points : {"",
                            "0,0",
                            "0,0 1",
                            "0,0 1,",
                            "0,0 ,1",
                            "0,0 x,1",
                            "0,0 1x,2",
                            "0,0 1,2x",
                            "0,0 1,2,3",
                            "0,0 nan,0",
                            "0,0 inf,0",
                            "0,0 1e999,0",
                            "0,0 90.0001,0",
                            "0,0 -90.0001,0",
                            "0,0 0,180.0001",
                            "0,0 0,-180.0001"}) {
    Scenario::Document held;
    held.Ground.Osm.push_back({.Kind = "retained"});
    const auto xml = Scene("way", points);
    std::string error;
    CHECK(!ReadScenario(xml.data(), xml.size(), held, error) && !error.empty(),
          "malformed coordinates cause a diagnostic, not partial success");
    CHECK(held.Ground.Osm.size() == 1 && held.Ground.Osm.front().Kind == "retained",
          "a late invalid feature preserves the previous document");
  }
  for (const auto points : {"0,0 1,1", " -90,-180 90,180 ", "0,0&#9;1e-7,-2E-7&#10;2,3"}) {
    Scenario::Document held;
    std::string error;
    const auto xml = Scene("way", points);
    CHECK(ReadScenario(xml.data(), xml.size(), held, error) && held.Ground.Osm.size() == 2,
          "canonical boundaries, exponents and XML whitespace are accepted");
  }
  for (const auto points : {"0,0 1,1", "0,0 1,1 2,0"}) {
    Scenario::Document held;
    std::string error;
    const auto xml = Scene("area", points);
    const bool accepted = ReadScenario(xml.data(), xml.size(), held, error);
    CHECK(accepted == (std::string_view(points) == "0,0 1,1 2,0"),
          "areas require at least three coordinate pairs");
  }
  std::string points;
  for (int i = 0; i < 65536; ++i) { points += "0,0 "; }
  for (const bool excess : {false, true}) {
    if (excess) { points += "1,1"; }
    const auto xml = Scene("way", points);
    Scenario::Document held;
    std::string error;
    CHECK(ReadScenario(xml.data(), xml.size(), held, error) != excess,
          "explicit point budget accepts its boundary and refuses the next point");
  }
  return Report();
}
