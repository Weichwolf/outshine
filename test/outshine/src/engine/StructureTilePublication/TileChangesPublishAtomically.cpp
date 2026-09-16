#include "StructureTilePublication.h"
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
    const auto ground = base.addSurface("ground", Material{});
    const auto wall = base.addSurface("wall", Material{});
    const auto roof = base.addSurface("roof", Material{});
    CHECK(ground && wall && roof, "fixture materials exist");
    if (!ground || !wall || !roof) { return Report(); }
    const auto part = base.addPart("base", *ground);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "base geometry is valid");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "initial world opens");
    if (scene) {
      Surrounds world;
      world.BindLiveResources(*scene);
      world.Pieces.Wears({.Walls = static_cast<uint32_t>(wall->index()),
                          .Roofs = static_cast<uint32_t>(roof->index())});
      Generators::BakedTile built;
      built.Built.WallCorners = {StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
                                 StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
                                 StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
      built.Built.WallRun = {0, 1, 2};
      built.Built.RoofCorners = built.Built.WallCorners;
      built.Built.RoofRun = {0, 1, 2};
      built.Digest = 19;
      StructureBakes::Landing landing{.Tile = 7, .Baked = &built, .AnchorEcef = {}};
      CHECK(PublishStructureTile(world, renderer, scene, landing, nullptr).has_value() &&
                renderer.PiecesStanding() == 2,
            "complete tile publishes wall and roof");
      const auto original = scene.get();
      const auto digest = world.Pieces.Digest();
      const auto payload = scene->PieceSourceBytes();
      world.Pieces.Wears({.Walls = static_cast<uint32_t>(wall->index()), .Roofs = 3});
      CHECK(!PublishStructureTile(world, renderer, scene, landing, nullptr),
            "replacement roof refuses after its candidate wall uploads");
      CHECK(scene.get() == original && renderer.PiecesStanding() == 2 &&
                world.Pieces.Digest() == digest && world.Pieces.Handed() == 1 &&
                scene->PieceSourceBytes() == payload,
            "rejected tile keeps old world, geometry, digest and source payload");
      world.Pieces.Wears({.Walls = static_cast<uint32_t>(wall->index()),
                          .Roofs = static_cast<uint32_t>(roof->index())});
      for (size_t count : {size_t{1}, size_t{2}}) {
        Generators::BakedTile malformed;
        malformed.Built.WallCorners = built.Built.WallCorners;
        malformed.Built.WallRun.resize(count, 0);
        landing.Baked = &malformed;
        CHECK(!PublishStructureTile(world, renderer, scene, landing, nullptr) &&
                  scene.get() == original && renderer.PiecesStanding() == 2 &&
                  world.Pieces.Handed() == 1 && world.Pieces.Digest() == digest,
              "an incomplete triangle is an error, not an empty tile replacement");
      }
      Generators::BakedTile empty;
      landing.Baked = &empty;
      CHECK(PublishStructureTile(world, renderer, scene, landing, nullptr).has_value(),
            "empty tile is an accepted replacement");
      CHECK(renderer.PiecesStanding() == 0 && scene->PieceSourceBytes() == 0 &&
                world.Pieces.Handed() == 2 && world.Pieces.Digest() != digest,
            "empty replacement removes previous buildings and advances publication");
      landing.Baked = &built;
      CHECK(PublishStructureTile(world, renderer, scene, landing, nullptr).has_value() &&
                renderer.PiecesStanding() == 2 && world.Pieces.Handed() == 3,
            "buildings can return after an empty revision");
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
          world.BindLiveResources(*scene);
        }
      }
      world.Pieces.Forgets(7);
      CHECK(renderer.PiecesStanding() == 0 && scene->PieceSourceBytes() == 0,
            "streaming owner still addresses published pieces after all replacements");
    }
  }
  SDL_Quit();
  return Report();
}
