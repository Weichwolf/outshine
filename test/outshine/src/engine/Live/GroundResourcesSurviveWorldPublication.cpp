#include "Live.h"
#include "WorldCandidate.h"
#include "EngineHeld.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry geometry;
    const int part = geometry.addPart("triangle", MaterialInstance{}).value();
    CHECK(geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "replacement geometry is valid");
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "ground fixture opens");
    if (scene) {
      Surrounds world;
      world.BindLiveResources(*scene);
      std::vector<float> nodes(Render::GroundLattice::kPageNodes, 3.0f);
      const auto removed = scene->PlaceHeightPage(nodes);
      CHECK(removed.has_value(), "a predecessor creates a gap in native slot identity");
      if (!removed) { return Report(); }
      const auto page = scene->PlaceHeightPage(nodes);
      CHECK(page.has_value(), "a stable height-page handle is placed");
      if (!page) { return Report(); }
      scene->ReleaseHeightPage(*removed);
      std::vector<float> fractions(Render::GroundLattice::kSide);
      for (size_t at = 0; at < fractions.size(); ++at) {
        fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
      }
      Core::GroundTile tile;
      tile.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
      tile.Page = *page;
      tile.LowM = 3.0f;
      tile.HighM = 3.0f;
      CHECK(scene->SetGroundGrid(fractions, error) &&
                scene->SetGroundLattice({&tile, 1}, {}, error),
            "ground inputs retain stable page handles");
      CHECK(renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
            "one published ground tile has its full topology");
      std::unique_ptr<Core::Live> candidate;
      CHECK(Core::Live::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error) &&
                Core::Live::PublishesPreparedWorld(renderer, scene, candidate, error),
            "an empty declared world publishes its first streamed-ground candidate");
      world.BindLiveResources(*scene);
      CHECK(renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
            "first streamed-world publication retains the ground tile topology");
      CHECK(Core::Live::ReplacesGeometry(renderer, *scene, geometry.clone(), nullptr, scene, error),
            "geometry replacement recreates ground resources in its candidate");
      world.BindLiveResources(*scene);
      CHECK(renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
            "candidate publication retains the ground tile topology");
      scene->ReleaseHeightPage(*page);
      const auto replacement = scene->PlaceHeightPage(nodes);
      CHECK(replacement && replacement->Slot == page->Slot &&
                replacement->Generation != page->Generation,
            "a replaced height page uses a new generation in the same native slot");
      if (!replacement) { return Report(); }
      Core::GroundTile next = tile;
      next.Page = *replacement;
      CHECK(scene->SetGroundLattice({&next, 1}, {}, error), "replacement page publishes");
      CHECK(!scene->SetGroundLattice({&tile, 1}, {}, error) &&
                renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
            "stale lattice input is rejected without changing the current topology");
      for (const Core::HeightPageHandle invalid :
           {Core::HeightPageHandle{},
            Core::HeightPageHandle{.Slot = Core::kNoResourceSlot - 1, .Generation = 1},
            Core::HeightPageHandle{.Slot = replacement->Slot, .Generation = 0}}) {
        Core::GroundTile refused = next;
        refused.Page = invalid;
        CHECK(!scene->SetGroundLattice({&refused, 1}, {}, error) &&
                  renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
              "invalid page identities preserve the published lattice");
        scene->ReleaseHeightPage(invalid);
      }
      const auto payload = scene->HeightPageSourceBytes();
      scene->ReleaseHeightPage(*page);
      CHECK(scene->HeightPageSourceBytes() == payload,
            "stale page release cannot remove its successor");
      {
        Core::WorldCandidate rejected(renderer);
        const auto prepared = rejected.Prepare(*scene, nullptr);
        CHECK(prepared && rejected.Scene().SetGroundLattice({&next, 1}, {}, error),
              "candidate reconstruction preserves the new page generation");
      }
      CHECK(scene->SetGroundLattice({&next, 1}, {}, error),
            "candidate rejection keeps published page identity usable");
      Core::WorldCandidate successor(renderer);
      CHECK(successor.Prepare(*scene, nullptr) && successor.Publish(scene) &&
                scene->SetGroundLattice({&next, 1}, {}, error),
            "a second candidate publishes the reused native generation");
      world.BindLiveResources(*scene);
      world.Sheets.Clear();
      CHECK(renderer.GroundLatticeTriangles() == 0,
            "rebound streaming owner clears the current world after two replacements");
    }
  }
  SDL_Quit();
  return Report();
}
