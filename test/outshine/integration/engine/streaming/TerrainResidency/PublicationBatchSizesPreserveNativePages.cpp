#include "TerrainPublicationFixture.h"
#include "TerrainLattice.h"
#include "Check.h"
#include "TerrainRenderer.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "streaming/TerrainResidency.h"

#include <SDL3/SDL.h>

#include <chrono>
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
      constexpr uint32_t firstTileX = 8192;
      constexpr uint32_t tileCount = 3;
      constexpr float firstAslM = 2;
      Patchwork patch;
      for (uint32_t x = firstTileX; x < firstTileX + tileCount; ++x) {
        patch.Sheets.push_back(
            {.Tile = {.Zoom = 14, .X = x, .Y = 8192},
             .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes,
                                         static_cast<float>(x - firstTileX) + firstAslM),
             .Side = Render::TerrainLattice::kSide,
             .Postings = Render::TerrainLattice::kSide});
      }
      const TangentFrame frame = TangentFrame::At({});
      TerrainResidency staged;
      staged.Into(&renderer);
      CHECK(!staged.AdvancePublish(patch, frame, 1),
            "advancing without a prepared publication fails");
      CHECK(staged.BeginPublish(patch, error), "complete tile set validates before staging");
      for (size_t step = 0; step < 2; ++step) {
        const auto advanced = staged.AdvancePublish(patch, frame, 1);
        CHECK(advanced && !*advanced && renderer.TerrainRendererTriangles() == 0,
              "partial height pages do not expose partial terrain tiles");
      }
      const auto completed = staged.AdvancePublish(patch, frame, 1);
      CHECK(completed && *completed && renderer.TerrainRendererTriangles() > 0,
            "last slice publishes the complete terrain set");
      const uint64_t digest = staged.Digest();
      const size_t bytes = renderer.HeightPageSourceBytes();
      Patchwork repeated = patch;
      repeated.Sheets.push_back(patch.Sheets.front());
      CHECK(!staged.BeginPublish(repeated, error) && staged.Digest() == digest &&
                renderer.HeightPageSourceBytes() == bytes,
            "duplicate identity rejects before changing staged or visible resources");
      staged.Clear();
      CHECK(renderer.TerrainRendererTriangles() == 0, "clear removes the staged publication");

      const auto offlineUploads = renderer.OfflineUploads(std::chrono::seconds(1));
      TerrainResidency fullBatch;
      fullBatch.Into(&renderer);
      CHECK(PublishTerrain(fullBatch, patch, frame, error) && fullBatch.Digest() == digest &&
                renderer.HeightPageSourceBytes() == bytes,
            "full-batch and page-wise publication have the same native product");
      CHECK(PublishTerrain(fullBatch, {}, frame, error) && renderer.TerrainRendererTriangles() == 0,
            "empty publication releases all terrain pages");
      fullBatch.Clear();
    }
  }
  SDL_Quit();
  return Report();
}

#include <cstddef>
#include <cstdint>
