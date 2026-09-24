#include "EngineHeld.h"

#include <utility>

namespace outshine {

namespace Says {
constexpr auto NoRenderTarget = "a render target is required before creating the scene";
}

bool Engine::State::EnsureRuntimeScene() {
  if (!Picture.Standing) {
    if (!Picture.Targeted) {
      Error = Says::NoRenderTarget;
      return false;
    }
    Core::Declaration wanted = Picture.Shown;
    wanted.SurfaceWidthPx = Picture.Frame.WidthPx;
    wanted.SurfaceHeightPx = Picture.Frame.HeightPx;
    if (Picture.PendingGeometry) { wanted.InitialGeometry = &*Picture.PendingGeometry; }
    if (!Core::RuntimeScene::Open(
            Picture.Device, std::move(wanted), &Picture.Face, Picture.Standing, Error)) {
      return false;
    }
    Picture.PendingGeometry.reset();
    if (Picture.PendingAudioOcclusion) {
      World.AudioOcclusion = std::move(*Picture.PendingAudioOcclusion);
      Picture.PendingAudioOcclusion.reset();
    }
  }
  if (!Picture.PendingGeometry) { return true; }
  if (!Core::RuntimeScene::ReplacesGeometry(Picture.Device,
                                            *Picture.Standing,
                                            Picture.PendingGeometry->clone(),
                                            &Picture.Face,
                                            Picture.Standing,
                                            Error)) {
    return false;
  }
  World.BindSceneResources(Picture.Device);
  Picture.PendingGeometry.reset();
  if (Picture.PendingAudioOcclusion) {
    World.AudioOcclusion = std::move(*Picture.PendingAudioOcclusion);
    Picture.PendingAudioOcclusion.reset();
  }
  return true;
}

}
