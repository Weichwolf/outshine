#include "Check.h"
#include "GroundWorldCandidate.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  Render::SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "empty initial world opens");
  if (scene) {
    Surrounds world;
    Ground::BuildingField footprints;
    world.BindSceneResources(renderer);
    Geometry ground;
    const auto earth = ground.addSurface("earth", Material{});
    const auto part =
        earth ? ground.addPart("ground", *earth) : std::expected<int, GeometryPartError>{};
    CHECK(earth && part &&
              ground.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              ground.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "future terrain geometry is valid");
    Generators::BakedTile baked;
    baked.Built.WallCorners = {StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 1, 0}}),
                               StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 1, 0}}),
                               StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 1, 0}})};
    baked.Built.WallRun = {0, 1, 2};
    baked.Built.RoofCorners = baked.Built.WallCorners;
    baked.Built.RoofRun = {0, 1, 2};
    baked.Digest = 17;
    {
      GroundWorldCandidate candidate(renderer, world, footprints);
      CHECK(candidate.Prepare(*scene, nullptr).has_value(),
            "candidate prepares before terrain exists");
      Geometry materials;
      CHECK(materials.addSurface("walls", Material{}) && materials.addSurface("roofs", Material{}),
            "structure palette has two materials");
      const auto first = renderer.RegisterPieceMaterials(std::move(materials));
      CHECK(first && *first == 0, "first registered structure slot starts at zero");
      if (first) {
        auto &pieces = candidate.Products().Pieces;
        pieces.Wears({.Walls = Render::PieceSurface::Registered(*first),
                      .Roofs = Render::PieceSurface::Registered(*first + 2u)});
        CHECK(!pieces.Hands(7, baked, {}, error) && renderer.PiecesStanding() == 0,
              "invalid registered roof rejects the whole tile");
        candidate.Products().Surfaces =
            TilePieces::Surfaces{.Walls = Render::PieceSurface::Registered(*first),
                                 .Roofs = Render::PieceSurface::Registered(*first + 1u)};
        pieces.Wears(*candidate.Products().Surfaces);
        CHECK(pieces.Hands(7, baked, {}, error) && renderer.PiecesStanding() == 2,
              "walls and roofs upload before terrain geometry exists");
      }
      CHECK(candidate.SetGroundGeometry(ground.clone(), 0, error),
            "terrain mesh arrives after the structure pieces");
      CHECK(candidate.Publish(world, footprints, scene, {.Region = 1}).has_value() &&
                renderer.PiecesStanding() == 2,
            "ground and early structure pieces publish together");
    }
    {
      GroundWorldCandidate candidate(renderer, world, footprints);
      CHECK(candidate.Prepare(*scene, nullptr).has_value(),
            "next candidate restores the registered materials and pieces");
      CHECK(candidate.Products().Surfaces &&
                candidate.Products().Surfaces->Walls.From ==
                    Render::PieceSurface::Source::Registered &&
                candidate.Products().Surfaces->Walls.Index == 0 && renderer.PiecesStanding() == 2,
            "second revision reuses the same structure slots");
      CHECK(candidate.SetGroundGeometry(ground.clone(), 0, error),
            "second revision rebuilds its terrain subject");
      CHECK(candidate.Publish(world, footprints, scene, {.Region = 2}).has_value() &&
                renderer.PiecesStanding() == 2,
            "second publication retains both early structure pieces");
    }
    world.Pieces.Forgets(7);
    CHECK(renderer.PiecesStanding() == 0, "published piece handles release correctly");
  }
  SDL_Quit();
  return Report();
}
