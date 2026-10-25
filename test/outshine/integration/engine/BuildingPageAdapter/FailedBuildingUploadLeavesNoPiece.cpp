#include <cstddef>
#include <cstdint>
#include <string>
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "GeometryPageResidency.h"
#include "BuildingPageAdapter.h"
#include <SDL3/SDL.h>
#include <array>
#include <limits>
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
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "fixture opens");
    if (scene) {
      Generators::BakedTile baked;
      baked.Built.WallCorners = {StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 1, 0}}),
                                 StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 1, 0}}),
                                 StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 1, 0}})};
      baked.Built.WallRun = {0, 1, 2};
      baked.Built.RoofCorners = baked.Built.WallCorners;
      baked.Built.RoofRun = {0, 1, 2};
      Render::GeometryPageResidency pieces;
      BuildingPageMaterials materials;
      pieces.BindRenderer(&renderer);
      materials = {.Walls = Render::PieceSurface(0), .Roofs = Render::PieceSurface(1)};
      CHECK(!pieces.Upload(7, BuildingPageView(baked, materials).View(), {}, error),
            "invalid roof surface rejects the complete tile");
      CHECK(pieces.Uploads() == 0 && renderer.PiecesStanding() == 0,
            "a rejected roof removes its already resident wall");
      materials = {.Walls = Render::PieceSurface(0), .Roofs = Render::PieceSurface(0)};
      CHECK(pieces.Upload(7, BuildingPageView(baked, materials).View(), {}, error) &&
                renderer.PiecesStanding() == 2,
            "a complete original tile installs its wall and roof");
      const size_t ownerBytes = pieces.HeapBytes();
      CHECK(ownerBytes > 0, "piece owner counts its retained handle slots and diagnostic storage");
      const uint64_t originalDigest = pieces.Digest();
      const auto validCorners = baked.Built.WallCorners;
      for (StoredVertex &corner : baked.Built.WallCorners) { corner.texture = {{0, 0}}; }
      CHECK(!pieces.Upload(7, BuildingPageView(baked, materials).View(), {}, error) &&
                renderer.PiecesStanding() == 2 && pieces.Digest() == originalDigest,
            "a completely absent facade coordinate field cannot replace visible geometry");
      baked.Built.WallCorners = validCorners;
      baked.Built.WallCorners.front().texture = {{std::numeric_limits<float>::quiet_NaN(), 0}};
      CHECK(!pieces.Upload(7, BuildingPageView(baked, materials).View(), {}, error) &&
                renderer.PiecesStanding() == 2 && pieces.Digest() == originalDigest,
            "a nonfinite facade coordinate cannot replace visible geometry");
      baked.Built.WallCorners = validCorners;
      baked.Digest = 7;
      materials = {.Walls = Render::PieceSurface(0), .Roofs = Render::PieceSurface(1)};
      CHECK(!pieces.Upload(7, BuildingPageView(baked, materials).View(), {}, error),
            "replacement roof refuses after its wall uploads");
      CHECK(renderer.PiecesStanding() == 2 && pieces.Uploads() == 1 &&
                pieces.Digest() == originalDigest,
            "failed replacement preserves both old pieces and the published digest");
      materials = {.Walls = Render::PieceSurface(0), .Roofs = Render::PieceSurface(0)};
      CHECK(pieces.Upload(7, BuildingPageView(baked, materials).View(), {}, error) &&
                renderer.PiecesStanding() == 2 && pieces.Uploads() == 2 &&
                pieces.Digest() != originalDigest,
            "retry replaces both pieces once without leaking old or refused geometry");
      const size_t beforeRetireBytes = pieces.HeapBytes();
      pieces.Retire(7);
      CHECK(renderer.PiecesStanding() == 0,
            "forgetting the accepted replacement releases both pieces");
      CHECK(beforeRetireBytes - pieces.HeapBytes() >= 2 * sizeof(Render::PieceHandle),
            "retiring a page releases the separately owned mesh-handle array");
    }
  }
  SDL_Quit();
  return Report();
}
