#include "TerrainLattice.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "streaming/TerrainResidency.h"
#include "Check.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {
void CheckCandidateRestoration(outshine::Render::SceneRenderer &renderer,
                               outshine::Core::RuntimeScene &scene,
                               outshine::Test::Gpu::UploadFailure &failure) {
  using namespace outshine;
  using namespace outshine::Test::Gpu;
  Core::WorldCandidate candidate(renderer);
  auto opened = candidate.Prepare(scene,
                                  nullptr,
                                  Render::SceneResources::PieceSources::Copy,
                                  Core::GeometryRetention::All,
                                  Core::ResourceRestoreMode::Deferred);
  CHECK(opened, "replacement opens with the published terrain inputs");
  if (!opened) { return; }
  failure.Point = UploadFailurePoint::Acquire;
  failure.Kind = Render::Native::ErrorKind::Busy;
  failure.Skip = 1;
  failure.Armed = true;
  const auto waits = failure.Waits;
  const auto submissions = failure.AcceptedSubmissions;
  size_t nextPage = 0;
  auto restored = candidate.AdvanceGroundResourceRestore(nextPage, 1);
  CHECK(restored && !*restored && nextPage == 1 && !failure.Armed &&
            failure.AcceptedSubmissions == submissions + 1,
        "instance upload defers after admitting the page and reusing the grid");
  if (!restored) { return; }
  restored = candidate.AdvanceGroundResourceRestore(nextPage, 1);
  CHECK(restored && *restored && nextPage == 1 && failure.Waits == waits &&
            failure.AcceptedSubmissions == submissions + 2 &&
            renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u,
        "retry uploads only the instances and completes the candidate without waiting");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Test::Gpu;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    UploadFailure failure{.Message = "temporary terrain upload backpressure"};
    Render::SceneRenderer renderer(
        {.Context = &failure, .Create = NativeUploadFaultDevice::Create});
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      CHECK(renderer.Settle(error), "fixture setup completes before fault injection");
      const auto waits = failure.Waits;
      Patchwork patch;
      patch.Sheets.push_back({.Tile = {.Zoom = 14, .X = 8192, .Y = 8192},
                              .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, 3),
                              .Side = Render::TerrainLattice::kSide,
                              .Postings = Render::TerrainLattice::kSide + 1});
      const TangentFrame frame = TangentFrame::At({});
      TerrainResidency residency;
      residency.Into(&renderer);
      CHECK(residency.BeginPublish(patch, error), "bounded terrain publication begins");
      const auto defer = [&](UploadFailurePoint point, unsigned skip) {
        failure.Point = point;
        failure.Skip = skip;
        failure.Kind = Render::Native::ErrorKind::Busy;
        failure.Armed = true;
        const auto result = residency.AdvancePublish(patch, frame, 1);
        CHECK(result && !*result && !failure.Armed,
              "native backpressure defers the same publication without becoming an engine error");
        CHECK(failure.Waits == waits && renderer.TerrainRendererTriangles() == 0,
              "deferred frame work never waits or exposes an incomplete terrain set");
      };
      defer(UploadFailurePoint::Acquire, 0);
      CHECK(residency.TileCount() == 0 && renderer.HeightPageSourceBytes() == 0,
            "refused command admission creates no resident page or retained CPU payload");
      defer(UploadFailurePoint::Submit, 0);
      CHECK(residency.TileCount() == 0 && renderer.HeightPageSourceBytes() == 0,
            "refused submission preserves the page cursor and permits an exact retry");
      defer(UploadFailurePoint::Acquire, 1);
      const auto bytes = renderer.HeightPageSourceBytes();
      CHECK(residency.TileCount() == 1 &&
                bytes == Render::TerrainLattice::kPageNodes * sizeof(float),
            "accepted page survives a later grid-upload refusal");
      defer(UploadFailurePoint::Acquire, 2);
      CHECK(residency.TileCount() == 1 && renderer.HeightPageSourceBytes() == bytes &&
                renderer.HeightPageSlots() == 1,
            "accepted grid and page survive instance-upload refusal without page duplication");
      const auto vertices = failure.VertexBufferAllocations;
      const auto completed = residency.AdvancePublish(patch, frame, 1);
      CHECK(completed && *completed && failure.Rejected == 4 && failure.Waits == waits &&
                renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u &&
                renderer.HeightPageSourceBytes() == bytes,
            "the original bounded transaction resumes and publishes its complete topology");
      CHECK(failure.VertexBufferAllocations == vertices + 1,
            "retry allocates the refused instance buffer without rebuilding the accepted grid");
      constexpr LongitudeLatitude point{.LongitudeDeg = 0.001, .LatitudeDeg = -0.001};
      const auto height = residency.HeightMAt(point);
      CHECK(height.has_value(), "the committed height page supports a real world query");
      patch.Sheets.front().Nodes.assign(Render::TerrainLattice::kPageNodes, 7);
      CHECK(residency.BeginPublish(patch, error), "a replacement begins after recovery");
      CHECK(renderer.Settle(error), "accepted prior uploads finish before the real map fault");
      failure.Kind = Render::Native::ErrorKind::Driver;
      failure.Point = UploadFailurePoint::Map;
      failure.Armed = true;
      const auto broken = residency.AdvancePublish(patch, frame, 1);
      CHECK(!broken && broken.error() == failure.Message && residency.HeightMAt(point) == height &&
                renderer.HeightPageSourceBytes() == bytes &&
                renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u,
            "a real driver error stays an error and preserves the committed terrain");
      CheckCandidateRestoration(renderer, *scene, failure);
      residency.Clear();
    }
  }
  SDL_Quit();
  return Report();
}
