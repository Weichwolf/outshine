#include "GroundWorldCandidate.h"

#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "published world opens");
    Surrounds world;
    Ground::BuildingField footprints;
    world.BindSceneResources(renderer);
    std::array<Render::HeightPageHandle, 3> pages;
    const std::vector<float> nodes(Render::GroundLattice::kPageNodes, 2.0f);
    for (Render::HeightPageHandle &page : pages) {
      const auto placed = renderer.PlaceHeightPage(nodes);
      CHECK(placed.has_value(), "published height page uploads");
      if (placed) { page = *placed; }
    }
    if (scene) {
      {
        GroundWorldCandidate candidate(renderer, world, footprints);
        auto advanced = candidate.AdvancePreparation(*scene, nullptr, 1);
        CHECK(advanced && !*advanced,
              "first advance creates the candidate without restoring pages");
        for (size_t restored = 0; restored < pages.size(); ++restored) {
          advanced = candidate.AdvancePreparation(*scene, nullptr, 1);
          CHECK(advanced && (*advanced == (restored + 1 == pages.size())),
                "each advance restores exactly one source page before completion");
        }
      }
      CHECK(renderer.HasHeightPage(pages[0]) && renderer.HasHeightPage(pages[1]) &&
                renderer.HasHeightPage(pages[2]),
            "abandoning the paced candidate preserves every published page handle");
    }
  }
  SDL_Quit();
  return Report();
}
