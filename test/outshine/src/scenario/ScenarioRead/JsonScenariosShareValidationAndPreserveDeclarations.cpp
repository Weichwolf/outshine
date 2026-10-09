#include "Check.h"
#include "ScenarioJson.h"
#include "ScenarioRead.h"
#include "ScenarioWrite.h"

#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view source = R"({
    "name":"JSON & \"world\"",
    "world":{"lat":47.232575,"lon":9.598371,"vegetation":false,"sightM":240000},
    "render":{"widthPx":640,"heightPx":480,"antialiasing":"temporal"},
    "clock":{"start":"2026-09-07T10:40:00Z","live":false},
    "views":{"view":[
      {"id":"main","person":"first","at":{"lat":47.232575,"lon":9.598371,
       "heightM":614,"samplesHeight":false,"bearingDeg":1,"pitchDeg":-11.5}},
      {"id":"second","person":"first","at":{"lat":47.232575,"lon":9.598371,
       "heightM":615,"samplesHeight":false,"bearingDeg":2,"pitchDeg":-10}}
    ]}
  })";
  Scenario::Document scene;
  std::string error;
  CHECK(ReadScenario(source.data(), source.size(), scene, error),
        "JSON reads through scenario validation");
  CHECK(scene.Named.Name == "JSON & \"world\"" && !scene.Ground.VegetationEnabled &&
            scene.Ground.Origin.LatitudeDeg == 47.232575 && scene.Render.Frame.WidthPx == 640 &&
            scene.Views.size() == 2 && scene.Views.back().Id == "second",
        "scalars, booleans, escaped strings and ordered collections survive");
  const auto xml = WriteScenario(scene);
  CHECK(xml.has_value(), "JSON declarations serialize using the shared scenario writer");
  if (!xml) { return Report(); }
  Scenario::Document replay;
  CHECK(ReadScenario(xml->data(), xml->size(), replay, error), "XML replay succeeds");
  CHECK(WriteScenario(replay) == xml, "JSON and XML produce the same canonical declaration");

  constexpr std::string_view overrides =
      R"({"world":{"vegetation":true},"render":{"widthPx":1280},"views":{"view":[{"id":"replacement"}]},"name":null})";
  const auto patched = ScenarioXmlFromJson(overrides, *xml);
  CHECK(patched.has_value(),
        "JSON override merges objects, replaces arrays and removes null members");
  if (patched) {
    CHECK(ReadScenario(patched->data(), patched->size(), replay, error),
          "patched declaration validates");
    CHECK(replay.Ground.VegetationEnabled && replay.Ground.Origin.LatitudeDeg == 47.232575 &&
              replay.Render.Frame.WidthPx == 1280 && replay.Render.Frame.HeightPx == 480 &&
              replay.Views.size() == 1 && replay.Views.front().Id == "replacement" &&
              replay.Named.Name.empty(),
          "partial overrides preserve unrelated fields without appending collections");
  }
  for (std::string_view invalid : {R"([])",
                                   R"({"world":{"unused":1}})",
                                   R"({"unknown":{}})",
                                   R"({"name":"a","name":"b"})",
                                   R"({"world":{"lat":true}})",
                                   R"({"views":{"view":[1,{}]}})",
                                   R"({"bad\"key":0})",
                                   R"({"name":"\u0000"})",
                                   R"({"name":)"}) {
    const auto before = WriteScenario(scene);
    CHECK(!ReadScenario(invalid.data(), invalid.size(), scene, error) && !error.empty(),
          "malformed, unknown, mistyped and duplicate JSON fields are rejected");
    CHECK(WriteScenario(scene) == before, "failed JSON never publishes a partial declaration");
  }
  const auto ambiguous = ScenarioXmlFromJson(R"({"views":{"view":{"id":"ambiguous"}}})", *xml);
  CHECK(!ambiguous, "an object cannot silently choose one repeated child");
  return Report();
}
