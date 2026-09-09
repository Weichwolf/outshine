#include "ScenarioRead.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const char *value : {"0", "-1", "1.5", "2147483648", "1e300", "nan", "inf", "8x", ""}) {
    const std::string text =
        std::string("<scenario><physics mostStepsInArrears=\"") + value + "\"/></scenario>";
    Scenario::Document scene;
    std::string error;
    CHECK(!ReadScenario(text.data(), text.size(), scene, error) &&
              error.find("mostStepsInArrears requires") != std::string::npos,
          "invalid count is refused before integer conversion");
  }
  for (const char *value : {"1", "8.0", "2147483647"}) {
    const std::string text =
        std::string("<scenario><physics mostStepsInArrears=\"") + value + "\"/></scenario>";
    Scenario::Document scene;
    std::string error;
    CHECK(ReadScenario(text.data(), text.size(), scene, error),
          "representable integer is accepted");
  }
  Xml layer;
  const std::string text = "<scenario><physics stepS=\"0.02\"/></scenario>";
  CHECK(layer.Parse(text.data(), text.size()), "layer fixture parses");
  Scenario::Document scene;
  scene.Motion.MostStepsInArrears = 3;
  std::string error;
  CHECK(ReadSectionsOnto(layer.Root(), scene, error) && scene.Motion.MostStepsInArrears == 3,
        "omitted count preserves the inherited value");
  return Report();
}
