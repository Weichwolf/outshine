#include "FramePipeline.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <string>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr float kTestIlluminanceLx = 120;
  constexpr float kMeteringConstant = 2.5f;
  constexpr float kCameraCalibration = 1.2f;
  constexpr float kTolerance = 1e-8f;
  constexpr float kExplicitExposure = 0.25f;
  FramePipelineSettings settings;
  settings.IlluminanceLx = kTestIlluminanceLx;
  const auto metered = BuildFramePipeline(settings);
  CHECK(metered &&
            std::abs(metered->Exposure.Or(0) -
                     kMeteringConstant / (kCameraCalibration * kTestIlluminanceLx)) < kTolerance,
        "incident-light metering preserves the calibrated camera response");
  settings.Exposure = kExplicitExposure;
  settings.Transfer = "linear";
  settings.Precision = "float";
  const auto explicitPlan = BuildFramePipeline(settings);
  CHECK(explicitPlan && explicitPlan->Exposure.Or(0) == kExplicitExposure &&
            explicitPlan->Display.Or(Transfer::Filmic) == Transfer::Linear &&
            explicitPlan->Precision.Or(ScenePrecision::Half) == ScenePrecision::Float,
        "explicit camera and storage settings override the default pipeline");
  settings.Antialiasing = "unknown";
  CHECK(!BuildFramePipeline(settings), "unknown antialiasing is refused");
  settings.Antialiasing = "none";
  settings.Transfer = "unknown";
  CHECK(!BuildFramePipeline(settings), "unknown transfer is refused");
  settings.Transfer = "linear";
  settings.Precision = "unknown";
  CHECK(!BuildFramePipeline(settings), "unknown precision is refused");
  settings.Precision = "half";
  const std::array<std::string, 1> unknown{"unknown"};
  settings.Outputs = unknown;
  CHECK(!BuildFramePipeline(settings), "unknown output is refused");
  settings.Outputs = {};
  settings.Stages = unknown;
  CHECK(!BuildFramePipeline(settings), "unknown stage is refused");
  settings.Stages = {};
  CHECK(BuildFramePipeline(settings), "a refused settings value cannot poison a later plan");
  return Report();
}
