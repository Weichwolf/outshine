#include "TerrainLattice.h"
#include "RuntimeScene.h"
#include "WorldCandidate.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

void CheckEmptyFeedback(Render::SceneRenderer &renderer) {
  std::vector<Render::FieldPageAddress> pages(renderer.GroundFieldSamples());
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  bool finished = false;
  do {
    const auto ready = renderer.TakeGroundFieldRequests(pages);
    CHECK(ready, "visible terrain feedback decodes without interpreting clear alpha as a key");
    if (!ready) { return; }
    if (*ready) {
      CHECK(**ready == 0,
            "unclassified terrain and uncovered background request no material pages");
      finished = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  CHECK(finished, "nonblocking feedback completes within the test deadline");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    const auto uploads = renderer.OfflineUploads(std::chrono::seconds(5));
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface", "groundFieldSamples"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
    if (!scene) { return Report(); }
    const std::vector<float> nodes(Render::TerrainLattice::kPageNodes, 3.0f);
    const auto page = renderer.PlaceHeightPage(nodes);
    CHECK(page, "a native ground page is available");
    if (!page) { return Report(); }
    std::vector<float> fractions(Render::TerrainLattice::kSide);
    for (size_t at = 0; at < fractions.size(); ++at) {
      fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1);
    }
    Render::TerrainTile tile;
    tile.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
    tile.Page = *page;
    tile.LowM = tile.HighM = 3;
    CHECK(renderer.SetGroundGrid(fractions, error) &&
              renderer.SetTerrainTiles({&tile, 1}, {}, error),
          error.c_str());
    auto eye = Render::Viewpoint::LookAt({.EyeM = {{0, 7, 8}}, .AimM = {{0, 3, 0}}}, 0.0);
    CHECK(eye, "the camera sees native ground and uncovered background");
    if (!eye) { return Report(); }
    eye->YfovRad = 1;
    eye->ZNearM = 0.1;
    eye->ZFarM = 100;
    scene->Eye(*eye);
    CHECK(scene->Draw(error) && renderer.TerrainRendererTriangles() > 0, error.c_str());
    CHECK(renderer.GroundFields() != nullptr && renderer.GroundFieldSamples() == 16,
          "the explicit feedback profile owns its atlas and bounded sample grid");
    CheckEmptyFeedback(renderer);
    const auto *const original = renderer.GroundFields();
    Core::WorldCandidate successor(renderer);
    CHECK(successor.Prepare(*scene, nullptr) && successor.Publish(scene),
          "a new world publication reuses the configured frame resources");
    CHECK(renderer.SetTerrainTiles({&tile, 1}, {}, error) && scene->Draw(error), error.c_str());
    CHECK(renderer.GroundFields() == original,
          "world publication retains the same device-owned page atlas");
    CheckEmptyFeedback(renderer);
  }
  SDL_Quit();
  return Report();
}
