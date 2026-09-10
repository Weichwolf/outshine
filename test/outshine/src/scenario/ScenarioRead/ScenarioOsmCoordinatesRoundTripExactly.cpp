#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::vector<std::vector<double>> rings{
      {48.208123456789,
       16.373987654321,
       48.208123556789,
       16.373987754321,
       48.208123656789,
       16.373987654321},
      {-33.8688123456789,
       151.2093123456789,
       -33.8688122456789,
       151.2093124456789,
       -33.8688121456789,
       151.2093123456789},
      {0.0, -0.0, 0.00000001, -0.00000002, 0.00000003, -0.00000004},
      {90, 180, std::nextafter(90.0, 0.0), std::nextafter(180.0, 0.0), -90, -180}};
  for (const char *relief : {"", "flat"}) {
    for (const bool area : {false, true}) {
      for (const auto &points : rings) {
        Scenario::Document original;
        original.Ground.Declared = true;
        original.Ground.Shape.Kind = relief;
        Scenario::Structure feature;
        feature.Area = area;
        feature.Kind = area ? "building" : "residential";
        feature.LatLon = points;
        original.Ground.Osm.push_back(feature);
        const auto text = WriteScenario(original);
        Scenario::Document restored;
        std::string error;
        const bool read = ReadScenario(text.data(), text.size(), restored, error);
        CHECK(read, error.empty() ? "serialized OSM parses" : error.c_str());
        CHECK(restored.Ground.Osm.size() == 1, "one input feature stays one feature");
        if (restored.Ground.Osm.size() != 1) { continue; }
        CHECK(restored.Ground.Shape.Kind == relief,
              "relief presence is preserved independently of OSM");
        const auto &actual = restored.Ground.Osm.front();
        CHECK(actual.Area == area && actual.Kind == feature.Kind,
              "feature classification survives");
        CHECK(std::ranges::equal(actual.LatLon,
                                 points,
                                 [](double a, double b) {
                                   return std::bit_cast<uint64_t>(a) == std::bit_cast<uint64_t>(b);
                                 }),
              "every original coordinate survives bit-exactly, including signed zero");
        CHECK(actual.LatLon[0] != actual.LatLon[2], "distinct input points do not collapse");
      }
    }
  }
  return Report();
}
