#include "WorldCandidate.h"

#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry base;
    const auto surface = base.addSurface("wall", Material{});
    CHECK(surface.has_value(), "fixture material exists");
    if (!surface) { return Report(); }
    const auto part = base.addPart("base", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "fixture has a surface");
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "published world opens");
    if (scene) {
      const std::array<StoredVertex, 3> vertices{
          StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
      const std::array<uint32_t, 3> indices{0, 1, 2};
      const Render::PieceMesh mesh{.Verts = vertices, .Indices = indices};
      std::array<Render::PieceHandle, 3> handles;
      for (auto &handle : handles) {
        const auto placed = renderer.PlacePiece(mesh);
        CHECK(placed.has_value(), "published piece uploads");
        if (placed) { handle = *placed; }
      }
      Render::PieceHandle latePublished;
      {
        Core::WorldCandidate candidate(renderer);
        const auto prepared = candidate.Prepare(*scene,
                                                nullptr,
                                                Render::SceneResources::PieceSources::Copy,
                                                Core::SubjectGeometrySources::All,
                                                Core::ResourceRestoreMode::Deferred);
        CHECK(prepared.has_value(), "candidate copies immutable piece sources");
        if (prepared) {
          CHECK(renderer.HasWorldCandidate() && renderer.PieceSlots() == handles.size(),
                "the candidate editor addresses the copied resource table");
          {
            const auto published = renderer.PublishedWorld();
            const auto placed = renderer.PlacePiece(mesh);
            CHECK(placed.has_value(), "the published world accepts an independent late piece");
            if (placed) { latePublished = *placed; }
            CHECK(renderer.PieceSlots() == handles.size() + 1,
                  "the scoped resource edit targets the published table");
          }
          CHECK(renderer.PieceSlots() == handles.size(),
                "leaving the scope returns to the candidate table");
          size_t next = 0;
          CHECK(!candidate.AdvancePieceResourceRestore(next, 0), "zero budget is rejected");
          CHECK(next == 0, "rejected budget preserves cursor");
          for (size_t restored = 0; restored < handles.size(); ++restored) {
            const auto advanced = candidate.AdvancePieceResourceRestore(next, 1);
            CHECK(advanced && (*advanced == (restored + 1 == handles.size())) &&
                      next == restored + 1,
                  "each advance restores exactly one piece before completion");
          }
          CHECK(candidate.AdvancePieceResourceRestore(next, 1).value_or(false),
                "completed restoration is idempotent");
          {
            const auto published = renderer.PublishedWorld();
            CHECK(renderer.SetPieceInstances(latePublished, {}, error),
                  "published instances remain addressable during candidate restoration");
          }
        }
      }
      CHECK(!renderer.HasWorldCandidate(), "abandoning the candidate closes its resource table");
      for (const auto handle : handles) {
        CHECK(renderer.SetPieceInstances(handle, {}, error),
              "abandoning candidate preserves published piece handles");
      }
      CHECK(renderer.SetPieceInstances(latePublished, {}, error),
            "the scoped published piece remains live after candidate abandonment");
      renderer.ReleasePiece(latePublished);
    }
  }
  SDL_Quit();
  return Report();
}
