#include <string>
#include <expected>
#include <cstdint>
#include <utility>
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
    WorldRegionState world;
    ::outshine::Generators::Osm::BuildingField footprints;
    world.BindSceneResources(renderer);
    Geometry ground;
    const auto earth = ground.addSurface("earth", Material{});
    const auto part =
        earth ? ground.addPart("ground", *earth) : std::expected<int, GeometryPartError>{};
    CHECK(earth && part &&
              ground.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              ground.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "future terrain geometry is valid");
    constexpr float kInteriorUv = 0.1f;
    constexpr uint64_t kBakedDigest = 17;
    Generators::BakedTile baked;
    baked.Built.WallCorners = {
        StoredVertex::Of({{0, 0, 0}}, {{kInteriorUv, kInteriorUv}}, {{0, 1, 0}}),
        StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 1, 0}}),
        StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 1, 0}})};
    baked.Built.WallRun = {0, 1, 2};
    baked.Built.RoofCorners = baked.Built.WallCorners;
    baked.Built.RoofRun = {0, 1, 2};
    baked.Digest = kBakedDigest;
    {
      GroundWorldCandidate candidate(renderer, world, footprints);
      CHECK(candidate.Prepare(*scene, nullptr).has_value(),
            "candidate prepares before terrain exists");
      Geometry materials;
      CHECK(materials.addSurface("walls", Material{}) && materials.addSurface("roofs", Material{}),
            "structure palette has two materials");
      const auto first = renderer.RegisterPieceMaterials(std::move(materials));
      CHECK(first && first->First == 0, "first registered structure slot starts at zero");
      if (first) {
        auto &pieces = candidate.Products().Pieces;
        const BuildingPageMaterials invalid{.Walls = Render::PieceSurface::Registered(*first),
                                            .Roofs = Render::PieceSurface::Registered(*first, 2u)};
        CHECK(!pieces.Upload(7, BuildingPageView(baked, invalid).View(), {}, error) &&
                  renderer.PiecesStanding() == 0,
              "invalid registered roof rejects the whole tile");
        const BuildingPageMaterials valid{.Walls = Render::PieceSurface::Registered(*first),
                                          .Roofs = Render::PieceSurface::Registered(*first, 1u)};
        candidate.Products().Surfaces = valid;
        CHECK(pieces.Upload(7, BuildingPageView(baked, valid).View(), {}, error) &&
                  renderer.PiecesStanding() == 2,
              "walls and roofs upload before terrain geometry exists");
      }
      CHECK(candidate.SetGroundGeometry(ground.clone(), 0, error),
            "terrain mesh arrives after the structure pieces");
      CHECK(
          candidate.Publish(world, footprints, scene, {.Region = 1, .Coverage = {}}).has_value() &&
              renderer.PiecesStanding() == 2,
          "ground and early structure pieces publish together");
    }
    {
      CHECK(world.StructureSurfaces.has_value(),
            "published structure materials retain their source");
      if (!world.StructureSurfaces) { return Report(); }
      Render::PieceMesh extra{};
      extra.Verts = baked.Built.WallCorners;
      extra.Indices = baked.Built.WallRun;
      extra.Surface = world.StructureSurfaces->Walls;
      extra.Textured = true;
      const auto stray = renderer.PlaceStructurePiece(extra);
      const auto independent = renderer.PlacePiece(extra);
      CHECK(stray && independent && renderer.PiecesStanding() == 4,
            "a stale structure source and an unrelated source can coexist");
      if (!stray || !independent) { return Report(); }
      GroundWorldCandidate candidate(renderer, world, footprints);
      CHECK(candidate.Prepare(*scene, nullptr).has_value(),
            "next candidate restores the registered materials and pieces");
      const auto &surfaces = candidate.Products().Surfaces;
      CHECK(surfaces && surfaces->Walls.From == Render::PieceSurface::Source::Registered &&
                surfaces->Walls.Index == 0 && renderer.PiecesStanding() == 4,
            "second revision restores all registered piece sources before reconciliation");
      CHECK(candidate.SetGroundGeometry(ground.clone(), 0, error),
            "second revision rebuilds its terrain mesh");
      CHECK(
          candidate.Publish(world, footprints, scene, {.Region = 2, .Coverage = {}}).has_value() &&
              candidate.Publication().OrphanStructurePieces == 1 && renderer.PiecesStanding() == 3,
          "publication removes only the unowned structure source");
      if (independent) { renderer.ReleasePiece(*independent); }
    }
    world.Pieces.Retire(7);
    CHECK(renderer.PiecesStanding() == 0, "published piece handles release correctly");
  }
  SDL_Quit();
  return Report();
}
