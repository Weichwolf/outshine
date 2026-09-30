#include "Check.h"
#include "RuntimeScene.h"
#include "WorldCandidate.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  {
    Material back;
    back.Unlit = true;
    back.BaseColour = {{0.2f, 0.4f, 0.6f, 1}};
    Material clear;
    clear.Transmission = 1;
    clear.BaseColour = {{1, 1, 1, 1}};
    Geometry geometry;
    const auto bed = geometry.addSurface("bed", back).value();
    const auto water = geometry.addSurface("water", clear).value();
    for (int layer = 0; layer < 2; ++layer) {
      const auto part =
          geometry.addPart(layer == 0 ? "bed" : "water", layer == 0 ? bed : water).value();
      const float z = layer == 0 ? -0.5f : 0.0f;
      CHECK(geometry.setPositions(part,
                                  std::array<float, 12>{-1, -1, z, 1, -1, z, 1, 1, z, -1, 1, z}) &&
                geometry.setNormals(part,
                                    std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
                geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
            "two independent surfaces have valid geometry");
    }
    for (bool transmissive : {true, false}) {
      Render::SceneRenderer renderer;
      Core::Declaration declaration;
      declaration.InitialGeometry = &geometry;
      declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
      declaration.Outputs = {"sceneLinear"};
      std::unique_ptr<Core::RuntimeScene> scene;
      std::string error;
      CHECK(geometry.setSurface(water, transmissive ? clear : Material{}).has_value(),
            "initial material selects the published plan");
      CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
      if (!scene) { return Report(); }
      const auto draw = [&] {
        const auto published = renderer.PublishedWorld();
        std::vector<float> pixels;
        CHECK(scene->Draw(error), error.c_str());
        CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready,
              "complete linear frame is readable");
        return pixels;
      };
      const auto before = draw();
      for (bool publish : {false, true}) {
        Core::WorldCandidate candidate(renderer);
        const auto prepared = candidate.Prepare(*scene, nullptr);
        CHECK(prepared.has_value(), "candidate borrows the published world");
        if (!prepared) { return Report(); }
        Geometry middle = geometry.clone();
        CHECK(middle.setSurface(water, transmissive ? Material{} : clear).has_value(),
              "intermediate material requires a different plan");
        CHECK(candidate.SetGeometry(std::move(middle), 2, back, error), error.c_str());
        CHECK(draw() == before, "private frame resources do not change the published picture");
        CHECK(candidate.SetGeometry(geometry.clone(), 2, back, error), error.c_str());
        if (publish) {
          CHECK(candidate.Publish(scene).has_value(), "candidate returns to the initial plan");
          CHECK(draw() == before,
                "returning to the published plan retains its matching GPU targets");
        }
      }
      CHECK(draw() == before, "both cancellation and publication preserve the original frame");
    }
  }
  SDL_Quit();
  return Report();
}
