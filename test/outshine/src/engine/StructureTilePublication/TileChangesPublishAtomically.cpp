#include "StructureTilePublication.h"
#include "GroundWorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <span>

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
      built.Built.WallCorners = {StoredVertex::Of({{0, 0, 0}}, {{0.1f, 0.1f}}, {{0, 0, 1}}),
                                 StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
                                 StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
      built.Built.WallRun = {0, 1, 2};
      built.Built.RoofCorners = built.Built.WallCorners;
      built.Built.RoofRun = {0, 1, 2};
      built.Digest = 19;
      StructureBuildQueue::Landing landing{.Tile = 7, .Baked = &built, .AnchorEcef = {}};
      const auto original = scene.get();
      const auto first = PublishStructureTile(world, renderer, landing);
      CHECK(first.has_value(), first ? "first tile publishes" : first.error().c_str());
      CHECK(renderer.PiecesStanding() == 2 && scene.get() == original,
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
      landing.Tile = 10;
      landing.Baked = &built;
      landing.SourceKey = 41;
      built.OccupiedCells = 3;
      {
        Core::WorldCandidate retiring(renderer);
        CHECK(retiring.Prepare(*scene, nullptr).has_value(),
              "retiring ground candidate keeps a private renderer state");
        const auto live = PublishStructureTile(world, renderer, landing);
        CHECK(live.has_value(),
              live ? "live tile publishes during retirement" : live.error().c_str());
        CHECK(renderer.PiecesStanding() == 6,
              "live tile publication does not modify the private candidate");
        {
          const auto published = renderer.PublishedWorld();
          CHECK(renderer.PiecesStanding() == 8 && world.Pieces.ValidateSources(error),
                "published tile and source registry advance together");
        }
      }
      CHECK(renderer.PiecesStanding() == 8 && world.Pieces.ValidateSources(error),
            "abandoning the candidate preserves the live tile");
      const uint64_t legacyDigest = world.Pieces.Digest();
      Generators::BakedTile cellOne = built;
      cellOne.RequestedCell = 1;
      cellOne.RequestedDetail = LevelOfDetail::Fine;
      cellOne.OccupiedCells = 1;
      cellOne.Digest = 101;
      Generators::BakedTile cellTwo = built;
      cellTwo.RequestedCell = 2;
      cellTwo.RequestedDetail = LevelOfDetail::Shell;
      cellTwo.OccupiedCells = 2;
      cellTwo.Digest = 102;
      StructureBuildQueue::Landing cellLanding{.Tile = landing.Tile,
                                               .Baked = &cellOne,
                                               .AnchorEcef = landing.AnchorEcef,
                                               .SourceKey = landing.SourceKey};
      CHECK(StageStructureCell(world, renderer, cellLanding).has_value() &&
                renderer.PiecesStanding() == 10 && world.Pieces.Digest() == legacyDigest,
            "first cell lands without changing the published image");
      const std::array<TilePieces::CellSelection, 2> cells{
          {{.Cell = 1, .Detail = LevelOfDetail::Fine},
           {.Cell = 2, .Detail = LevelOfDetail::Shell}}};
      CHECK(!ActivateStructureCells(world, renderer, 10, 41, 3, cells) &&
                renderer.PiecesStanding() == 10 && world.Pieces.Digest() == legacyDigest,
            "incomplete cell set cannot retire the published whole tile");
      cellLanding.Baked = &cellTwo;
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(3)});
      CHECK(!StageStructureCell(world, renderer, cellLanding) && renderer.PiecesStanding() == 10 &&
                world.Pieces.Digest() == legacyDigest,
            "failed cell upload preserves the safety net and prior staged product");
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(static_cast<uint32_t>(roof->index()))});
      CHECK(StageStructureCell(world, renderer, cellLanding).has_value() &&
                renderer.PiecesStanding() == 12 &&
                !ActivateStructureCells(world, renderer, 10, 42, 3, cells) &&
                world.Pieces.Digest() == legacyDigest,
            "stale source cannot activate a complete staged cell set");
      CHECK(ActivateStructureCells(world, renderer, 10, 41, 3, cells).has_value() &&
                renderer.PiecesStanding() == 10 && world.Pieces.Digest() != legacyDigest,
            "complete source-matched cells atomically replace the published whole tile");
      size_t visibleCells = 0;
      world.Pieces.ForEachDigest([&](TilePieces::DigestRecord record) {
        if (record.Tile == 10 && record.Cell != 0) { ++visibleCells; }
      });
      CHECK(visibleCells == 2 && world.Pieces.ValidateSources(error),
            "cell publication retains two visible products with valid source handles");
      cellLanding.SourceKey = 42;
      cellLanding.Baked = &cellOne;
      CHECK(StageStructureCell(world, renderer, cellLanding).has_value() &&
                renderer.PiecesStanding() == 12,
            "new source cell stages while the prior revision remains visible");
      cellLanding.Baked = &cellTwo;
      CHECK(StageStructureCell(world, renderer, cellLanding).has_value() &&
                ActivateStructureCells(world, renderer, 10, 42, 3, cells).has_value() &&
                renderer.PiecesStanding() == 10 && world.Pieces.ValidateSources(error),
            "new source atomically replaces both cells and retires the prior GPU products");
      world.Pieces.ForEachDigest([&](TilePieces::DigestRecord record) {
        if (record.Tile == 10) { CHECK(record.SourceKey == 42, "only the new source is visible"); }
      });
      landing.SourceKey = 43;
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(3)});
      CHECK(!PublishStructureTile(world, renderer, landing) && renderer.PiecesStanding() == 10 &&
                world.Pieces.ValidateSources(error),
            "failed whole-tile fallback preserves published cell sources");
      world.Pieces.Wears({.Walls = Render::PieceSurface(static_cast<uint32_t>(wall->index())),
                          .Roofs = Render::PieceSurface(static_cast<uint32_t>(roof->index()))});
      CHECK(PublishStructureTile(world, renderer, landing).has_value() &&
                renderer.PiecesStanding() == 8 && world.Pieces.ValidateSources(error),
            "whole-tile source replacement retires both published cell products");
      world.Pieces.ForEachDigest([&](TilePieces::DigestRecord record) {
        if (record.Tile == 10) {
          CHECK(record.Cell == 0 && record.SourceKey == 43,
                "fallback publication exposes only the complete new tile");
        }
      });
      world.Pieces.Forgets(10);
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
