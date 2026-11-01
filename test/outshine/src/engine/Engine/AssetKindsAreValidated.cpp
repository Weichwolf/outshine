#include "ScenarioStartup.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document original;
  original.Motion.Declared = true;
  original.Motion.StepS = 0.025;
  CHECK(LoadFreshScenario(original).has_value(), "initial declaration succeeds");

  for (const auto *const kind : {"gltf", "unknown", ""}) {
    Scenario::Document candidate = original;
    candidate.Motion.StepS = 0.05;
    Scenario::Asset asset;
    asset.Kind = kind;
    candidate.Assets.push_back(asset);
    const auto result = LoadFreshScenario(candidate);
    CHECK(!result && !result.error().empty(), "unsupported asset kind is rejected");
  }
  CHECK(LoadFreshScenario(original).has_value(), "valid asset-free startup succeeds");
  return Report();
}
