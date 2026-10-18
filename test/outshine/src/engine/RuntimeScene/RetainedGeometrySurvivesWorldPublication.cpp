#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

outshine::Geometry Triangle(float x) {
  using namespace outshine;
  Geometry geometry;
  const auto part = geometry.addPart("retained", MaterialInstance{}).value();
  CHECK(geometry.setPositions(part, std::array<float, 9>{x - 1, -1, 0, x + 1, -1, 0, x, 1, 0}) &&
            geometry.setNormals(part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
            geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
        "immutable source has authored attributes");
  return geometry;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Surfacing.front().Unlit = true;
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::unique_ptr<Core::RuntimeScene> candidate;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (!scene) { return Report(); }
    CHECK(scene->SetGeometry(Triangle(0), 0, error), "generated geometry takes ownership");
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    constexpr double kNearPlaneM = 0.1;
    eye.ZNearM = kNearPlaneM;
    eye.ZFarM = 10;
    scene->Eye(eye);
    const auto pixels = [&]() {
      std::vector<float> values;
      CHECK(scene->Draw(error), "published world draws");
      renderer.WaitForGpu();
      CHECK(renderer.ReadSceneLinear(values) == Render::ReadState::Ready && !values.empty(),
            "published pixels are readable");
      return values;
    };
    const auto original = pixels();
    CHECK(Core::RuntimeScene::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error),
          "unchanged world prepares with retained source");
    CHECK(pixels() == original, "preparation leaves the published frame unchanged");
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    CHECK(pixels() == original, "abandoning a candidate preserves the source and published frame");
    CHECK(Core::RuntimeScene::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error),
          "a second world retains the source");
    CHECK(Core::RuntimeScene::PublishesPreparedWorld(renderer, scene, candidate, error),
          "publication destroys the old scene");
    CHECK(pixels() == original, "source attributes remain valid after the old scene is destroyed");
    CHECK(!Core::RuntimeScene::PreparesGeometryReplacement(
              renderer, *scene, Triangle(1), 2, nullptr, candidate, error),
          "invalid replacement rejects a mismatched driven count");
    CHECK(!candidate && pixels() == original,
          "failed replacement releases its source without changing the published world");
    CHECK(Core::RuntimeScene::PreparesGeometryReplacement(
              renderer, *scene, Triangle(1), 0, nullptr, candidate, error),
          "changed geometry gets an independent source");
    CHECK(pixels() == original, "independent candidate cannot overwrite published attributes");
    CHECK(Core::RuntimeScene::PublishesPreparedWorld(renderer, scene, candidate, error),
          "changed geometry publishes");
    CHECK(pixels() != original, "published independent source renders its changed geometry");
  }
  SDL_Quit();
  return Report();
}
