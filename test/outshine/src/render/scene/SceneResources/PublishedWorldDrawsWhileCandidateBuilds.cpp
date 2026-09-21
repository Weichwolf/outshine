#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry geometry;
    const int part = geometry.addPart("triangle", MaterialInstance{}).value();
    CHECK(geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "published fixture has visible geometry");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Surfacing.front().Unlit = true;
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "published world opens");
    if (scene) {
      CHECK(scene->Draw(error), "published world draws before replacement");
      renderer.WaitForGpu();
      std::vector<float> expected;
      CHECK(renderer.ReadSceneLinear(expected) == Render::ReadState::Ready && !expected.empty(),
            "published pixels are readable");
      {
        Core::WorldCandidate candidate(renderer);
        const auto prepared = candidate.Prepare(*scene, nullptr);
        CHECK(prepared.has_value(), prepared ? "replacement prepares" : prepared.error().c_str());
        if (prepared) {
          CHECK(candidate.Scene().SetGeometry(Geometry{}, 0, error),
                "replacement may hold different geometry while it builds");
          CHECK(scene->Draw(error), "published world remains drawable during replacement");
          renderer.WaitForGpu();
        }
      }
      std::vector<float> actual;
      CHECK(renderer.ReadSceneLinear(actual) == Render::ReadState::Ready && actual == expected,
            "an unpublished candidate cannot affect the rendered frame");
    }
  }
  SDL_Quit();
  return Report();
}
