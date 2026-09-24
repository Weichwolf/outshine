#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"

#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const std::string_view valid = "<scenario name='routes'><routes>"
                                 "<route id='grand-prix' source='osm' relationId='284588'/>"
                                 "</routes></scenario>";
  Scenario::Document document;
  std::string error;
  CHECK(ReadScenario(valid.data(), valid.size(), document, error) && document.Routes.size() == 1 &&
            document.Routes[0].Id == "grand-prix" && document.Routes[0].OsmRelationId == 284588,
        "a named OSM relation is retained by scenario import");
  const auto written = WriteScenario(document);
  Scenario::Document copy;
  CHECK(written && ReadScenario(written->data(), written->size(), copy, error) &&
            copy.Routes == document.Routes,
        "scenario export preserves named route selectors");
  if (copy.Routes.empty()) { return Report(); }

  for (const std::string_view invalid :
       {"<scenario><routes><route id='x' source='osm' relationId='0'/></routes></scenario>",
        "<scenario><routes><route id='x' source='osm' relationId='1x'/></routes></scenario>",
        "<scenario><routes><route id='x' source='mvt' relationId='1'/></routes></scenario>",
        "<scenario><routes><route id='x' source='osm' relationId='1'/>"
        "<route id='x' source='osm' relationId='2'/></routes></scenario>",
        "<scenario><routes><route id='' source='osm' relationId='1'/></routes></scenario>"}) {
    CHECK(!ReadScenario(invalid.data(), invalid.size(), copy, error) &&
              copy.Routes == document.Routes && !error.empty(),
          "invalid route declaration preserves the previous document");
  }
  std::string tooMany = "<scenario><routes>";
  for (int id = 0; id < 33; ++id) {
    tooMany += "<route id='r" + std::to_string(id) + "' source='osm' relationId='1'/>";
  }
  tooMany += "</routes></scenario>";
  CHECK(!ReadScenario(tooMany.data(), tooMany.size(), copy, error) &&
            copy.Routes == document.Routes,
        "route count is bounded before a new document publishes");
  copy.Routes.push_back(copy.Routes.front());
  CHECK(!WriteScenario(copy), "writer rejects a programmatic duplicate route name");
  return Report();
}
