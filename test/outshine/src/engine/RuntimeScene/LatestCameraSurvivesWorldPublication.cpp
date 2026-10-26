#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr double kNearM = 0.125;
constexpr double kOffsetM = 1.5;
constexpr double kOrbitDegPerFrame = 30;

std::vector<float> Read(Render::SceneRenderer &renderer) {
  const auto published = renderer.PublishedWorld();
  renderer.WaitForGpu();
  std::vector<float> image;
  CHECK(renderer.ReadSceneLinear(image) == Render::ReadState::Ready && !image.empty(),
        "the published HDR frame is readable during candidate preparation");
  return image;
}

bool Prepare(Core::WorldCandidate &candidate, const Core::RuntimeScene &scene, const char *label) {
  const auto prepared = candidate.Prepare(scene, nullptr);
  CHECK(prepared, prepared ? label : prepared.error().c_str());
  return prepared.has_value();
}

void AnchorChanges(Render::SceneRenderer &renderer,
                   std::unique_ptr<Core::RuntimeScene> &scene,
                   const Geometry &geometry) {
  const auto image = Read(renderer);
  const auto expected = renderer.LastSubmittedCamera();
  const Vec3 expectedEye = expected.Basis.EyeM - expected.GeometryAnchorM;
  const auto input = scene->Watching();
  std::string error;
  for (const bool empty : {true, false}) {
    Core::WorldCandidate candidate(renderer);
    if (!Prepare(candidate, *scene, "the next anchor starts with the published world")) { return; }
    CHECK(candidate.SetGeometry(empty ? Geometry{} : geometry.clone(), empty ? 0 : 1, error),
          "empty and populated geometry use different native anchors");
    const auto previous = renderer.LastSubmittedCamera();
    CHECK(candidate.Publish(scene), "the replacement publishes across a changed anchor");
    CHECK(scene->Watching().EyeM == input.EyeM && scene->Aimed().EyeM == input.EyeM,
          "world publication preserves camera input in the engine's fixed coordinate frame");
    CHECK(renderer.LastSubmittedCamera().Serial == previous.Serial,
          "world publication does not submit an extra camera frame");
    CHECK(scene->Draw(error), "the rebound camera draws at the replacement anchor");
    const auto &submitted = renderer.LastSubmittedCamera();
    CHECK(submitted.GeometryAnchorM != previous.GeometryAnchorM,
          "the control actually changes the native geometry anchor");
    CHECK(submitted.Basis.EyeM - submitted.GeometryAnchorM == expectedEye &&
              submitted.Basis.Forward == expected.Basis.Forward &&
              submitted.Basis.Right == expected.Basis.Right &&
              submitted.Basis.Up == expected.Basis.Up,
          "changing render anchors preserves the camera in the paired geometry frame");
  }
  CHECK(Read(renderer) == image, "restoring geometry at its anchor restores the original view");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for live-view publication");
  {
    Geometry geometry;
    const int part = geometry.addPart("triangle", MaterialInstance{}).value();
    CHECK(geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "the same native triangle is retained across replacements");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.OrbitDegPerFrame = kOrbitDegPerFrame;
    Render::SceneRenderer renderer;
    const auto uploads = renderer.OfflineUploads(std::chrono::seconds(5));
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "the original world opens");
    if (!scene) { return Report(); }
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    eye.ZNearM = kNearM;
    eye.ZFarM = 16;
    scene->Eye(eye);
    CHECK(scene->Advance(error) && scene->Draw(error), "the first camera draws");
    for (const bool alreadyDrawn : {true, false}) {
      Core::WorldCandidate candidate(renderer);
      if (!Prepare(candidate, *scene, "world preparation captures the earlier view")) {
        return Report();
      }
      eye.EyeM[0] = alreadyDrawn ? kOffsetM : -kOffsetM;
      eye.XMagM = eye.YMagM = kOffsetM;
      scene->Eye(eye);
      std::vector<float> expected;
      if (alreadyDrawn) {
        CHECK(scene->Advance(error) && scene->Draw(error), "the newer published camera draws");
        expected = Read(renderer);
      }
      CHECK(candidate.Publish(scene), "the unchanged world publishes after the camera input");
      CHECK(scene->Aimed().EyeM == eye.EyeM && scene->Watching().EyeM == eye.EyeM,
            "publication retains both drawn and pending current camera input");
      CHECK(scene->Aimed().XMagM == eye.XMagM && scene->Aimed().YMagM == eye.YMagM &&
                scene->Aimed().ZNearM == eye.ZNearM && scene->Aimed().ZFarM == eye.ZFarM,
            "publication binds the latest lens before exposing the new world");
      CHECK(scene->Draw(error), "the replacement draws without another camera advance");
      const auto actual = Read(renderer);
      if (alreadyDrawn) {
        CHECK(actual == expected, "unchanged geometry preserves the latest camera's HDR image");
      }
    }
    const auto valid = scene->Aimed();
    const auto before = Read(renderer);
    Core::WorldCandidate candidate(renderer);
    if (!Prepare(candidate, *scene, "a candidate prepares before an invalid pending lens")) {
      return Report();
    }
    auto invalid = valid;
    invalid.XMagM = 0;
    scene->Eye(invalid);
    const auto *const original = scene.get();
    CHECK(!candidate.Publish(scene) && scene.get() == original,
          "invalid view binding cannot publish the prepared replacement");
    CHECK(Read(renderer) == before, "failed view binding preserves the last submitted image");
    scene->Eye(valid);
    CHECK(candidate.Publish(scene), "the same candidate retries with corrected live input");
    CHECK(scene->Aimed().EyeM == valid.EyeM && scene->Draw(error),
          "successful retry binds the corrected view without losing the candidate");
    AnchorChanges(renderer, scene, geometry);
    scene->FrameItself();
    CHECK(scene->Advance(error) && scene->Draw(error),
          "automatic framing draws before preparation");
    Core::WorldCandidate orbit(renderer);
    if (!Prepare(orbit, *scene, "a candidate captures the earlier automatic orbit")) {
      return Report();
    }
    const auto earlier = scene->Aimed();
    CHECK(scene->Advance(error) && scene->Draw(error), "the published automatic orbit advances");
    const auto latest = scene->Aimed();
    const auto orbited = Read(renderer);
    CHECK(latest.EyeM != earlier.EyeM, "the orbit control actually changes the current view");
    CHECK(orbit.Publish(scene), "the replacement publishes during automatic orbit");
    CHECK(!scene->Watched() && scene->Aimed().EyeM == latest.EyeM &&
              scene->Aimed().Forward == latest.Forward,
          "automatic framing retains the latest orbit without advancing it again");
    CHECK(scene->Draw(error) && Read(renderer) == orbited,
          "publication preserves the latest automatic orbit's HDR frame");
  }
  SDL_Quit();
  return Report();
}
