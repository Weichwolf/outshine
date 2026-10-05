#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for ground shadows");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
    declaration.DrawsSky = true;
    declaration.ShadowRadiusM = 256;
    declaration.KeyLux = 10000;
    declaration.KeyElevationDeg = 45;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "ground world opens");
    if (scene) {
      std::vector<float> nodes(GroundLattice::kPageNodes, 3.0f);
      const auto page = renderer.PlaceHeightPage(nodes);
      CHECK(page.has_value(), "native ground nodes are placed");
      if (!page) { return Report(); }
      std::vector<float> fractions(GroundLattice::kSide);
      for (size_t at = 0; at < fractions.size(); ++at) {
        fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
      }
      TerrainTile tile;
      tile.Corners = {{-64, -64, 64, -64, -64, 64, 64, 64}};
      tile.Page = *page;
      tile.LowM = tile.HighM = 3;
      CHECK(renderer.SetGroundGrid(fractions, error) &&
                renderer.SetTerrainTiles({&tile, 1}, {}, error),
            "ground tile is complete");
      Viewpoint eye;
      eye.EyeM = {{0, 0, 100}};
      eye.YfovRad = 1;
      eye.ZNearM = 0.1;
      eye.ZFarM = 2000;
      scene->Eye(eye);
      renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 256, true);
      CHECK(scene->Draw(error), "a world with no subject vertex buffer casts its ground");
      renderer.WaitForGpu();
      std::vector<float> first;
      CHECK(renderer.ReadShadowAtlas(first) == ReadState::Ready &&
                std::ranges::any_of(first, [](float depth) { return depth > 0.0f; }),
            "terrain has real caster depth independently of subject geometry");
      CHECK(renderer.SetTerrainTiles({}, {}, error),
            "ground is removed without replacing subjects");
      CHECK(scene->Draw(error), "the modified ground world renders");
      renderer.WaitForGpu();
      std::vector<float> second;
      CHECK(renderer.ReadShadowAtlas(second) == ReadState::Ready && !second.empty() &&
                std::ranges::all_of(second, [](float depth) { return depth == 0.0f; }),
            "terrain changes invalidate and clear the submitted shadow cache");
    }
  }
  SDL_Quit();
  return Report();
}
