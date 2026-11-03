#include "Check.h"
#include "TerrainPublicationFixture.h"
#include "TerrainClipmapRequest.h"
#include "TerrainSurfacePublication.h"
#include "ClipmapHeightCache.h"
#include "TerrainLattice.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Tasks.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <semaphore>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kWorkBytes = size_t{1} * 1024 * 1024;

void CheckSampler(const TerrainSurfacePublication &surface, const TangentFrame &frame) {
  const auto point =
      frame.ToLocalPosition({.LongitudeDeg = 0.02, .LatitudeDeg = -0.02, .HeightM = 2});
  const std::array<Vec2, 2> positions{{{{point.EastM, point.NorthM}}, {{-100000, 0}}}};
  std::array<float, 2> heights{};
  CHECK(surface.SampleRenderHeights(positions, heights, 1).has_value(),
        "published native heights sample without GPU or source IO");
  CHECK(std::abs(heights[0] - point.UpM) < 1e-5f && std::isnan(heights[1]),
        "the forward ECEF point retains height; missing native coverage stays unknown");
  std::stop_source stop;
  (void)stop.request_stop();
  const auto cancelled = surface.SampleRenderHeights(positions, heights, 1, stop.get_token());
  CHECK(!cancelled && cancelled.error() == TerrainSamplingError::Cancelled,
        "cancelled sampling stops before publishing");
  CHECK(!surface.SampleRenderHeights(positions, std::span(heights).first(1), 1),
        "source queries require matching output storage");
}

void CheckWorker(TerrainSurfacePublication &surface, const TangentFrame &frame) {
  Tasks pool(1);
  std::binary_semaphore release{0};
  const auto barrier = pool.Post([&] { release.acquire(); });
  CHECK(barrier != Tasks::kNoTask, "one compute worker accepts a held queue entry");
  const auto point = frame.ToLocalGroundPosition({.LongitudeDeg = 0.02, .LatitudeDeg = -0.02});
  auto cache = Render::ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 1}, kWorkBytes);
  CHECK(cache.has_value(), "bounded render height storage is admitted");
  if (!cache) {
    release.release();
    return;
  }
  const Vec2 camera{{point.EastM, point.NorthM}};
  auto query = cache->Plan(camera, kWorkBytes);
  CHECK(query.has_value(), "the owner selects its sample coordinates");
  if (!query) {
    release.release();
    return;
  }
  const auto source = std::make_shared<const TerrainSurfacePublication>(surface);
  TerrainClipmapRequest request(
      pool, source, std::move(*query), {.SpacingM = 1, .SamplesPerSlice = 17});
  CHECK(!request.Finished() && !request.Take(), "the frame can poll pending work without waiting");
  surface.Clear();
  CheckSampler(*source, frame);
  release.release();
  pool.Wait(barrier);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!request.Finished() && std::chrono::steady_clock::now() < deadline) {
    (void)pool.AwaitCompletion(0.001);
  }
  auto sampled = request.Take();
  CHECK(sampled && sampled->has_value(),
        "cooperative worker sampling finishes from its held publication");
  if (!sampled || !sampled->has_value()) { return; }
  CHECK(cache->Commit(**sampled).has_value() && cache->Complete(0),
        "only the owner commits the prepared update");
  CHECK(!request.Take(), "completed data transfers exactly once");
  const auto origin = cache->Origin(0);
  CHECK(origin.has_value(), "sampled window publishes its origin");
  if (origin) {
    for (uint32_t y = 0; y < cache->Side(); ++y) {
      for (uint32_t x = 0; x < cache->Side(); ++x) {
        const auto height = cache->Height(0, {(*origin)[0] + x, (*origin)[1] + y});
        CHECK(height && *height < 2 && *height > 0,
              "native samples include curvature instead of a flat altitude plane");
      }
    }
  }
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
      const auto frame = TangentFrame::At({});
      TerrainSurfacePublication surface;
      surface.SetFrame(frame);
      surface.SetRenderer(&renderer);
      Patchwork patch;
      patch.Sheets.push_back({.Tile = {.Zoom = 12, .X = 2048, .Y = 2048},
                              .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, 2),
                              .Side = Render::TerrainLattice::kSide,
                              .Postings = Render::TerrainLattice::kSide});
      CHECK(PublishTerrain(surface, patch, error), error.c_str());
      CheckSampler(surface, frame);
      CheckWorker(surface, frame);
    }
  }
  SDL_Quit();
  return Report();
}
