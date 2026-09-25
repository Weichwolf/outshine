#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for world replacement");
  {
    Geometry geometry;
    const int part = geometry.addPart("caster", MaterialInstance{}).value();
    CHECK(geometry.setPositions(part,
                                std::array<float, 24>{-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
                                                      -1, -1, 1,  1, -1, 1,  1, 1, 1,  -1, 1, 1}) &&
              geometry.setTriangles(part,
                                    std::array<uint32_t, 36>{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                                             0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
                                                             0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5}),
          "closed caster geometry is valid");

    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
    declaration.DrawsSky = true;
    declaration.ShadowRadiusM = 8;
    declaration.KeyLux = 10000;
    declaration.KeyElevationDeg = 45;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "the caster world opens");
    if (scene) {
      Viewpoint eye;
      eye.EyeM = {{0, 0, 5}};
      eye.YfovRad = 1;
      eye.ZNearM = 0.1;
      eye.ZFarM = 100;
      scene->Eye(eye);
      renderer.CastsBelow(kNoBatch);
      CHECK(scene->Draw(error), "the first world draws");
      renderer.WaitForGpu();
      std::vector<float> first;
      CHECK(renderer.ReadShadowAtlas(first) == ReadState::Ready &&
                std::ranges::any_of(first, [](float depth) { return depth > 0.0f; }),
            "the first world casts a nonempty shadow atlas");

      Core::WorldCandidate candidate(renderer);
      const auto prepared = candidate.Prepare(*scene, nullptr);
      CHECK(prepared.has_value(), prepared ? "replacement prepares" : prepared.error().c_str());
      if (prepared) {
        CHECK(candidate.SetGeometry(Geometry{}, 0, error), "replacement removes the caster");
        CHECK(candidate.Publish(scene).has_value(), "replacement publishes");
        CHECK(scene->Draw(error), "the replacement world draws");
        renderer.WaitForGpu();
        std::vector<float> second;
        const ReadState read = renderer.ReadShadowAtlas(second);
        CHECK(read == ReadState::Ready, "the replacement atlas is available");
        CHECK(second.size() == first.size(), "the replacement atlas preserves its dimensions");
        CHECK(renderer.ShadowCastCount() == 0, "the replacement draws no shadow caster");
        CHECK(std::ranges::all_of(second, [](float depth) { return depth == 0.0f; }),
              "a published world without casters clears the previous shadow atlas");
      }
    }
  }
  SDL_Quit();
  return Report();
}
