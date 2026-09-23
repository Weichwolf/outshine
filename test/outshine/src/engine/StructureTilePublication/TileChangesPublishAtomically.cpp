#include "StructureTilePublication.h"
#include "GroundWorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry base;
    const auto groundSurface = base.addSurface("ground", Material{});
    const auto wall = base.addSurface("wall", Material{});
    const auto roof = base.addSurface("roof", Material{});
    CHECK(groundSurface && wall && roof, "fixture materials exist");
    if (!groundSurface || !wall || !roof) { return Report(); }
    const auto part = base.addPart("base", *groundSurface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "base geometry is valid");
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "initial world opens");
    if (scene) {
      Surrounds world;
      Ground::BuildingField footprints;
      world.BindSceneResources(renderer);
      GroundWorldCandidate ground(renderer, world, footprints);
      CHECK(ground.Prepare(*scene, nullptr).has_value(),
            "ground candidate prepares from an empty world");
      CHECK(ground.SetGroundGeometry(base.clone(), 0, error),
            "ground candidate carries every streamed piece material slot");
      CHECK(ground.Publish(world, footprints, scene, {.Region = 1}).has_value(),
            "ground candidate publishes its native material table");
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(static_cast<uint32_t>(roof->index()))});
      Generators::BakedTile built;
      built.Built.WallCorners = {StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
                                 StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
                                 StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
      built.Built.WallRun = {0, 1, 2};
      built.Built.RoofCorners = built.Built.WallCorners;
      built.Built.RoofRun = {0, 1, 2};
      built.Digest = 19;
      StructureBuildQueue::Landing landing{.Tile = 7, .Baked = &built, .AnchorEcef = {}};
      const auto original = scene.get();
      CHECK(PublishStructureTile(world, renderer, landing).has_value() &&
                renderer.PiecesStanding() == 2 && scene.get() == original,
            "complete tile publishes wall and roof without replacing the world");
      const auto digest = world.Pieces.Digest();
      const auto payload = renderer.PieceSourceBytes();
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(3)});
      CHECK(!PublishStructureTile(world, renderer, landing),
            "replacement roof refuses after its candidate wall uploads");
      CHECK(scene.get() == original && renderer.PiecesStanding() == 2 &&
                world.Pieces.Digest() == digest && world.Pieces.Handed() == 1 &&
                renderer.PieceSourceBytes() == payload,
            "rejected tile keeps old world, geometry, digest and source payload");
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(static_cast<uint32_t>(roof->index()))});
      for (size_t count : {size_t{1}, size_t{2}}) {
        Generators::BakedTile malformed;
        malformed.Built.WallCorners = built.Built.WallCorners;
        malformed.Built.WallRun.resize(count, 0);
        landing.Baked = &malformed;
        CHECK(!PublishStructureTile(world, renderer, landing) && scene.get() == original &&
                  renderer.PiecesStanding() == 2 && world.Pieces.Handed() == 1 &&
                  world.Pieces.Digest() == digest,
              "an incomplete triangle is an error, not an empty tile replacement");
      }
      Generators::BakedTile empty;
      landing.Baked = &empty;
      CHECK(PublishStructureTile(world, renderer, landing).has_value(),
            "empty tile is an accepted replacement");
      CHECK(renderer.PiecesStanding() == 0 && renderer.PieceSourceBytes() == 0 &&
                world.Pieces.Handed() == 2 && world.Pieces.Digest() != digest,
            "empty replacement removes previous buildings and advances publication");
      landing.Baked = &built;
      CHECK(PublishStructureTile(world, renderer, landing).has_value() &&
                renderer.PiecesStanding() == 2 && world.Pieces.Handed() == 3,
            "buildings can return after an empty revision");
      std::array<StructureBuildQueue::Landing, 2> batch{
          {{.Tile = 8, .Baked = &built, .AnchorEcef = {}},
           {.Tile = 9, .Baked = &built, .AnchorEcef = {}}}};
      CHECK(PublishStructureTile(world, renderer, batch[0]).has_value() &&
                PublishStructureTile(world, renderer, batch[1]).has_value() &&
                renderer.PiecesStanding() == 6 && world.Pieces.Handed() == 5,
            "independent complete tiles publish in order");
      {
        Core::WorldCandidate replacement(renderer);
        const auto prepared =
            replacement.Prepare(*scene, nullptr, Render::SceneResources::PieceSources::Omit);
        CHECK(prepared.has_value(), "a full structure rebuild prepares without old pieces");
        if (prepared) {
          CHECK(renderer.PiecesStanding() == 0 && renderer.PieceSourceBytes() == 0,
                "a full structure rebuild does not copy or upload obsolete pieces");
        }
      }
      CHECK(renderer.PiecesStanding() == 6,
            "abandoning the full rebuild leaves published pieces resident");
      const auto batchedDigest = world.Pieces.Digest();
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(3)});
      CHECK(!PublishStructureTile(world, renderer, batch[0]) && scene.get() == original &&
                renderer.PiecesStanding() == 6 && world.Pieces.Digest() == batchedDigest,
            "a tile failure preserves every published tile");
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(static_cast<uint32_t>(roof->index()))});
      {
        Core::WorldCandidate outer(renderer);
        const auto prepared = outer.Prepare(*scene, nullptr);
        CHECK(prepared.has_value(), "outer candidate prepares");
        if (prepared) {
          {
            Core::WorldCandidate nested(renderer);
            CHECK(!nested.Prepare(*scene, nullptr), "nested preparation is rejected");
          }
          CHECK(outer.Publish(scene).has_value(),
                "destroying a rejected nested candidate does not abandon its parent");
          world.BindSceneResources(renderer);
        }
      }
      world.Pieces.Forgets(7);
      world.Pieces.Forgets(8);
      world.Pieces.Forgets(9);
      CHECK(renderer.PiecesStanding() == 0 && renderer.PieceSourceBytes() == 0,
            "streaming owner still addresses published pieces after all replacements");
    }
  }
  SDL_Quit();
  return Report();
}
