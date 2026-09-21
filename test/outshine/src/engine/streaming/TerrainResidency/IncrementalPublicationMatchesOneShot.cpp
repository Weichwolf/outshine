#include "Check.h"
#include "GroundLattice.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TerrainResidency.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      Patchwork patch;
      for (uint32_t x = 8192; x < 8195; ++x) {
        patch.Sheets.push_back({.Tile = {.Zoom = 14, .X = x, .Y = 8192},
                                .Nodes = std::vector<float>(Render::GroundLattice::kPageNodes,
                                                            static_cast<float>(x - 8190)),
                                .Side = Render::GroundLattice::kSide,
                                .Postings = Render::GroundLattice::kSide});
      }
      const TangentFrame frame = TangentFrame::At({});
      TerrainResidency staged;
      staged.Into(&renderer);
      CHECK(!staged.AdvancePublish(patch, frame, 1),
            "advancing without a prepared publication fails");
      CHECK(staged.BeginPublish(patch, error), "complete tile set validates before staging");
      for (size_t step = 0; step < 2; ++step) {
        const auto advanced = staged.AdvancePublish(patch, frame, 1);
        CHECK(advanced && !*advanced && renderer.GroundLatticeTriangles() == 0,
              "partial height pages do not expose partial terrain tiles");
      }
      const auto completed = staged.AdvancePublish(patch, frame, 1);
      CHECK(completed && *completed && renderer.GroundLatticeTriangles() > 0,
            "last slice publishes the complete terrain set");
      const uint64_t digest = staged.Digest();
      const size_t bytes = renderer.HeightPageSourceBytes();
      Patchwork repeated = patch;
      repeated.Sheets.push_back(patch.Sheets.front());
      CHECK(!staged.BeginPublish(repeated, error) && staged.Digest() == digest &&
                renderer.HeightPageSourceBytes() == bytes,
            "duplicate identity rejects before changing staged or visible resources");
      staged.Clear();
      CHECK(renderer.GroundLatticeTriangles() == 0, "clear removes the staged publication");

      TerrainResidency oneShot;
      oneShot.Into(&renderer);
      CHECK(oneShot.Publish(patch, frame, error) && oneShot.Digest() == digest &&
                renderer.HeightPageSourceBytes() == bytes,
            "one-shot and sheet-wise publication have the same native product");
      CHECK(oneShot.Publish({}, frame, error) && renderer.GroundLatticeTriangles() == 0,
            "empty publication releases all terrain pages");
      oneShot.Clear();
    }
  }
  SDL_Quit();
  return Report();
}

#include <cstddef>
#include <cstdint>
