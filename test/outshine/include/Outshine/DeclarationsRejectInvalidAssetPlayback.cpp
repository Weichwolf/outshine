#include <Outshine.h>
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Engine engine;
  Scenario::Document original;
  original.Motion.Declared = true;
  original.Motion.StepS = 0.025;
  CHECK(engine.declare(original).has_value(), "initial declaration succeeds");
  const auto before = engine.writeScenario();
  CHECK(before.has_value(), "initial declaration exports");
  for (const bool invalidMode : {false, true}) {
    Scenario::Document candidate = original;
    candidate.Motion.StepS = 0.05;
    Scenario::Asset asset;
    asset.Clip = invalidMode ? 0 : -1;
    asset.Animation =
        invalidMode ? static_cast<Scenario::AssetAnimation>(255) : Scenario::AssetAnimation::Play;
    candidate.Assets.push_back(asset);
    const auto result = engine.declare(candidate);
    CHECK(!result && !result.error().empty(), "invalid playback rejected at public API");
    CHECK(engine.writeScenario() == before, "invalid playback preserves active declaration");
  }
  CHECK(engine.declare(original).has_value(), "valid retry remains usable");
  return Report();
}
