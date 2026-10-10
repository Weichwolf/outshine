#include "Check.h"
#include "ScenarioJson.h"
#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include <string>
#include <string_view>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::string_view input = R"({"render":{"terrain":false,"instances":false,
    "meshPart":[{"name":"streets"},{"name":"rail&platform"}]}})";
  Scenario::Document scene;
  std::string error;
  CHECK(ReadScenario(input.data(), input.size(), scene, error), error.c_str());
  CHECK(!scene.Render.Content.Terrain && !scene.Render.Content.Instances &&
            scene.Render.Content.MeshParts ==
                std::vector<std::string>({"streets", "rail&platform"}),
        "JSON selects native products with the same validated XML fields");
  const auto serialized = WriteScenario(scene);
  CHECK(serialized.has_value(), "native render selection serializes");
  if (!serialized) { return Report(); }
  Scenario::Document copy;
  CHECK(ReadScenario(serialized->data(), serialized->size(), copy, error) &&
            copy.Render.Content == scene.Render.Content,
        "native render selection survives import and export");
  const auto patched =
      ScenarioXmlFromJson(R"({"render":{"terrain":true,"meshPart":[]}})", *serialized);
  CHECK(patched.has_value(), "generic scenario overrides can clear native part selection");
  if (patched) {
    CHECK(ReadScenario(patched->data(), patched->size(), copy, error) &&
              copy.Render.Content.Terrain && !copy.Render.Content.Instances &&
              copy.Render.Content.MeshParts.empty(),
          "an explicit empty part array restores all mesh parts while preserving other fields");
  }
  return Report();
}
