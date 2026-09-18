#include <render/RenderConfiguration.h>

#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  constexpr Render::ImageRegion whole;
  static_assert(whole.whole());

  Render::Configuration request;
  CHECK(request.FrameRateHz == Render::kDefaultFrameRateHz &&
            request.CameraFill == Render::kDefaultCameraFill && request.Picture.whole(),
        "native render defaults require no scenario document");
  request.Outputs.emplace_back("sceneLinear");
  CHECK(request.Outputs.front() == "sceneLinear", "the native request owns renderer names");
  return Report();
}
