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
    Geometry base;
    const auto surface = base.addSurface("wall", Material{});
    CHECK(surface.has_value(), "fixture material is valid");
    const auto part = base.addPart("base", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "fixture geometry publishes exactly one available material");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
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
      pieces.Wears({.Walls = 0, .Roofs = 0});
      CHECK(pieces.Hands(7, baked, {}, error) && renderer.PiecesStanding() == 2,
            "a complete original tile installs its wall and roof");
      const uint64_t originalDigest = pieces.Digest();
      baked.Digest = 7;
      pieces.Wears({.Walls = 0, .Roofs = 1});
      CHECK(!pieces.Hands(7, baked, {}, error), "replacement roof refuses after its wall uploads");
      CHECK(renderer.PiecesStanding() == 2 && pieces.Handed() == 1 &&
                pieces.Digest() == originalDigest,
            "failed replacement preserves both old pieces and the published digest");
      pieces.Wears({.Walls = 0, .Roofs = 0});
      CHECK(pieces.Hands(7, baked, {}, error) && renderer.PiecesStanding() == 2 &&
                pieces.Handed() == 2 && pieces.Digest() != originalDigest,
            "retry replaces both pieces once without leaking old or refused geometry");
      pieces.Forgets(7);
      CHECK(renderer.PiecesStanding() == 0,
            "forgetting the accepted replacement releases both pieces");
    }
  }
  SDL_Quit();
  return Report();
}
