#include "Check.h"
#include "Live.h"
#include "SceneRenderer.h"
#include "TilePieces.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "fixture opens");
    if (scene) {
      Generators::BakedTile baked;
      baked.Built.WallCorners = {StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 1, 0}}),
                                 StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 1, 0}}),
                                 StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 1, 0}})};
      baked.Built.WallRun = {0, 1, 2};
      baked.Built.RoofCorners = baked.Built.WallCorners;
      baked.Built.RoofRun = {0, 1, 2};
      TilePieces pieces;
      pieces.Into(scene.get());
      pieces.Wears({.Walls = 0, .Roofs = 1});
      CHECK(!pieces.Hands(7, baked, {}, error), "invalid roof surface rejects the complete tile");
      CHECK(pieces.Handed() == 0 && renderer.PiecesStanding() == 0,
            "a rejected roof removes its already resident wall");
    }
  }
  SDL_Quit();
  return Report();
}
