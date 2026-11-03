#include "Check.h"
#include "TerrainClipmapPreparation.h"
#include "TerrainSurfacePublication.h"
#include "TerrainPublicationFixture.h"
#include "TerrainLattice.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Tasks.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstddef>
#include <memory>
#include <semaphore>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kBytes = size_t{1} * 1024 * 1024;
constexpr TerrainClipmapAdmission kAdmission{
    .WorkBytesMost = kBytes, .StagingBytesMost = kBytes, .SamplesPerSlice = 17};

void CheckCoverage(Render::SceneRenderer &renderer, const TerrainSurfacePublication &publication) {
  Tasks worker(1);
  auto fields = renderer.CreateTerrainClipmaps({.HalfCells = 8, .Levels = 1}, kBytes);
  CHECK(fields.has_value(), "native render fields are admitted");
  if (!fields) { return; }
  const Vec2 camera{{2000, -2000}};
  TerrainClipmapPreparation preparation(
      worker, std::make_shared<const TerrainSurfacePublication>(publication), *fields);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!preparation.Ready() && std::chrono::steady_clock::now() < deadline) {
    const auto advanced = preparation.Advance(camera, kAdmission);
    CHECK(advanced.has_value(), "bounded worker preparation advances without source IO");
    if (!advanced) { return; }
    (void)worker.AwaitCompletion(0.001);
  }
  CHECK(preparation.Ready(), "initial coverage requires two completely published fields");
  std::binary_semaphore release{0};
  const auto held = worker.Post([&] { release.acquire(); });
  CHECK(held != Tasks::kNoTask, "the compute worker accepts a held queue item");
  CHECK(preparation.Advance({{2002, -2000}}, kAdmission).has_value(),
        "movement queues new bands behind existing work");
  CHECK(preparation.Ready(), "pending replacement work preserves the accepted world coverage");
  release.release();
  worker.Wait(held);
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native publication fixture initializes video");
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
    if (scene) {
      TerrainSurfacePublication publication;
      publication.SetFrame(TangentFrame::At({}));
      publication.SetRenderer(&renderer);
      Patchwork patch;
      patch.Sheets.push_back({.Tile = {.Zoom = 12, .X = 2048, .Y = 2048},
                              .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, 2),
                              .Side = Render::TerrainLattice::kSide,
                              .Postings = Render::TerrainLattice::kSide});
      CHECK(PublishTerrain(publication, patch, error), error.c_str());
      CheckCoverage(renderer, publication);
    }
  }
  SDL_Quit();
  return Report();
}
