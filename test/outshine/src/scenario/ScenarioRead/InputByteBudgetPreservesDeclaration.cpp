#include "ScenarioRead.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::string text = "<scenario/>" + std::string(16u << 20u, ' ');
  Scenario::Document scene;
  scene.Named.Name = "preserved";
  std::string error;
  CHECK(!ReadScenario(text.data(), text.size(), scene, error),
        "in-memory scenario cannot bypass the XML input budget");
  CHECK(scene.Named.Name == "preserved", "rejected oversized scenario preserves prior state");
  return Report();
}
