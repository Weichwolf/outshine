#include "FramePipeline.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <string>
#include <vector>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  FramePipelineSettings settings;
  const auto opaque = BuildFramePipeline(settings);
  CHECK(opaque, "opaque settings produce a native pipeline without Engine or material objects");
  if (!opaque) { return Report(); }
  const auto base = ExecutionPlan::Compile(*opaque);
  CHECK(base && (*base)->Holds(Resource::SceneVelocity),
        "the default temporal-ready pipeline retains velocity");
  CHECK(base && !(*base)->Holds(Stage::CompositeTransmission) && !(*base)->Holds(Stage::Sky),
        "opaque scenes do not request sky or transmission");
  settings.DrawsSky = true;
  settings.Presents = true;
  settings.Transmissive = true;
  settings.Antialiasing = "temporal";
  const auto full = BuildFramePipeline(settings);
  CHECK(full, "sky, transparent surfaces, temporal resolve and presentation compose");
  if (!full) { return Report(); }
  const auto plan = ExecutionPlan::Compile(*full);
  CHECK(plan && (*plan)->Holds(Stage::Sky) && (*plan)->Holds(Stage::AerialPerspective) &&
            (*plan)->Holds(Stage::CompositeTransmission) && (*plan)->Holds(Stage::TemporalResolve),
        "the native execution plan resolves all requested scene contributions");
  CHECK(std::ranges::count(full->Outputs, Resource::Surface) == 1,
        "a presented pipeline requests its output surface once");
  const std::array<std::string, 1> stages{"opaqueGeometry"};
  const std::array<std::string, 2> outputs{"sceneLinear", "sceneLinear"};
  settings.Stages = stages;
  settings.Outputs = outputs;
  const auto explicitPlan = BuildFramePipeline(settings);
  CHECK(explicitPlan && std::ranges::count(explicitPlan->Outputs, Resource::SceneLinear) == 1,
        "explicit output requests are deduplicated");
  CHECK(explicitPlan && explicitPlan->Content ==
                            std::vector<Stage>({Stage::OpaqueGeometry, Stage::TemporalResolve}),
        "explicit stage selection remains authoritative while temporal resolve is requested");
  return Report();
}
