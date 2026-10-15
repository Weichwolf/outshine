#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TilePieces.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace {
void CheckSelection(outshine::Render::SceneRenderer &renderer) {
  using namespace outshine;
  using namespace outshine::Test;
  std::string error;
  Geometry materials;
  Material legacy;
  legacy.Pattern = SurfacePattern::Facade;
  Material metric = legacy;
  metric.Pattern = SurfacePattern::FacadeMetres;
  CHECK(materials.addSurface("legacy", legacy) && materials.addSurface("metres", metric),
        "both actual material modes register without images");
  const auto first = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(first.has_value(), "the wall material table registers");
  if (!first) { return; }
  Generators::BakedTile baked;
  baked.Digest = 1;
  baked.Built.WallCorners = {StoredVertex::Of({{0, 0, 0}}, {{0, 1}}, {{0, 0, 1}}),
                             StoredVertex::Of({{1, 0, 0}}, {{1, 1}}, {{0, 0, 1}}),
                             StoredVertex::Of({{0, 1, 0}}, {{0, 2}}, {{0, 0, 1}})};
  baked.Built.WallRun = {0, 1, 2};
  TilePieces pieces;
  pieces.Into(&renderer);
  constexpr uint32_t kMissingSurface = 99;
  pieces.SetSurfaces({.Walls = Render::PieceSurface::Registered(*first),
                      .Roofs = Render::PieceSurface::Registered(*first),
                      .MetreWalls = Render::PieceSurface::Registered(*first + kMissingSurface)});
  CHECK(pieces.Hands(1, baked, {}, error) && renderer.PiecesStanding() == 1,
        "an absent optional stream uses legacy walls even with an unavailable metre surface");
  const auto original = pieces.Digest();
  baked.Digest = 2;
  baked.Built.WallMaterialM = {0, 0, 1, 0, 0, 1};
  CHECK(!pieces.Hands(1, baked, {}, error) && renderer.PiecesStanding() == 1 &&
            pieces.Digest() == original,
        "actual metric data select the metre surface and failed admission keeps the old tile");
  pieces.SetSurfaces({.Walls = Render::PieceSurface::Registered(*first),
                      .Roofs = Render::PieceSurface::Registered(*first),
                      .MetreWalls = Render::PieceSurface::Registered(*first + 1u)});
  CHECK(pieces.Hands(1, baked, {}, error) && renderer.PiecesStanding() == 1,
        "the explicit metre material consumes the real UV1 stream without images");
  baked.Digest = 3;
  baked.Built.WallMaterialM.clear();
  CHECK(pieces.Hands(1, baked, {}, error) && renderer.PiecesStanding() == 1,
        "removing the optional stream selects the valid legacy material again");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native wall admission");
  {
    Geometry base;
    const auto surface = base.addSurface("base", Material{});
    CHECK(surface.has_value(), "the initial surface is valid");
    if (!surface) {
      SDL_Quit();
      return Report();
    }
    const auto part = base.addPart("base", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "the scene owns a complete native initial mesh");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
    if (scene) { CheckSelection(renderer); }
  }
  SDL_Quit();
  return Report();
}
