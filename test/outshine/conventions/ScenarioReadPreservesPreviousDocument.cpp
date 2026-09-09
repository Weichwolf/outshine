#include "ScenarioRead.h"
#include "Check.h"
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Named.Name = "previous";
  scene.Motion.StepS = 0.025;
  scene.Assets.push_back(Scenario::Asset{});
  scene.Assets.back().Uri = "previous.glb";
  std::string error;
  for (const std::string_view text : {"<scenario>",
                                      "<wrong/>",
                                      "<scenario><unknown/></scenario>",
                                      "<scenario name=\"new\"><assets><asset uri=\"new.glb\" "
                                      "kind=\"gltf\" animation=\"bad\"/></assets></scenario>",
                                      "<scenario name=\"new\"><tables><table id=\"t\"><column "
                                      "name=\"c\" type=\"bad\"/></table></tables></scenario>",
                                      "<scenario name=\"new\" unused=\"value\"/>"}) {
    CHECK(!ReadScenario(text.data(), text.size(), scene, error), "invalid input fails");
    CHECK(!error.empty(), "failure supplies a diagnosis");
    CHECK(scene.Named.Name == "previous" && scene.Motion.StepS == 0.025 &&
              scene.Assets.size() == 1 && scene.Assets.front().Uri == "previous.glb",
          "every failed parse preserves prior scalar and owned list content");
  }
  constexpr std::string_view valid = "<scenario name=\"replacement\"/>";
  CHECK(ReadScenario(valid.data(), valid.size(), scene, error), "valid retry succeeds");
  CHECK(scene.Named.Name == "replacement" && scene.Assets.empty() && error.empty(),
        "success publishes the replacement and clears the old diagnosis");
  return Report();
}
