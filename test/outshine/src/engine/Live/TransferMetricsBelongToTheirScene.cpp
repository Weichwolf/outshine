#include "Live.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video initialized");
  if (!initialized) { return Report(); }
  {
    Geometry geometry;
    const int part = geometry.addPart("triangle", MaterialInstance{}).value();
    CHECK(geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "native geometry ready");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Surfacing.front().Unlit = true;
    Render::SceneRenderer firstRenderer, secondRenderer;
    std::unique_ptr<Core::Live> first, second;
    std::string error;
    CHECK(Core::Live::Open(firstRenderer, declaration, nullptr, first, error), "first scene opens");
    CHECK(geometry.setPositions(part, std::array<float, 9>{-2, -1, 0, 2, -1, 0, 0, 2, 0}),
          "second scene uses different native geometry");
    CHECK(Core::Live::Open(secondRenderer, declaration, nullptr, second, error),
          "second scene opens");
    if (first && second) {
      const auto initialUploads = firstRenderer.TotalUploadAttempts();
      const auto otherUploads = secondRenderer.TotalUploadAttempts();
      CHECK(initialUploads > 0 && otherUploads > 0, "both renderers recorded initial uploads");
      CHECK(firstRenderer.TakeUploadAttempts() == initialUploads &&
                firstRenderer.TakeUploadAttempts() == 0 &&
                secondRenderer.TakeUploadAttempts() == otherUploads,
            "consuming one renderer upload count leaves the other intact");
      const auto firstBuffers = firstRenderer.TakeBufferAllocationAttempts();
      const auto secondBuffers = secondRenderer.TakeBufferAllocationAttempts();
      CHECK(firstBuffers > 0 && secondBuffers > 0 &&
                firstRenderer.TakeBufferAllocationAttempts() == 0,
            "buffer allocation counters are independent and consumed once");
      CHECK(firstRenderer.TakeStagingAllocationAttempts() > 0 &&
                secondRenderer.TakeStagingAllocationAttempts() > 0 &&
                firstRenderer.TakeStagingAllocationAttempts() == 0,
            "staging allocation counters belong to their renderer");
      CHECK(firstRenderer.TakeUploadBytes() > 0 && secondRenderer.TakeUploadBytes() > 0 &&
                firstRenderer.TakeUploadBytes() == 0,
            "byte accounting retains sub-megabyte uploads and is consumed independently");
      first->Digests(true);
      CHECK(first->Carries(2, error), "first scene uploads with digest enabled");
      CHECK(firstRenderer.TotalUploadAttempts() > initialUploads &&
                secondRenderer.TotalUploadAttempts() == otherUploads,
            "first mesh upload changes only its residency");
      const auto beforeUploads = firstRenderer.TotalUploadAttempts();
      const auto before = first->TransferMetrics();
      CHECK(before.GeometryDigest != 0 && std::isfinite(before.PackingMs) &&
                before.PackingMs >= 0 && std::isfinite(before.DigestMs) && before.DigestMs >= 0 &&
                std::isfinite(before.UploadMs) && before.UploadMs >= 0,
            "first scene exposes a digest and finite CPU phase timings");
      second->Digests(false);
      CHECK(second->Carries(2, error), "second scene uploads without digest work");
      CHECK(firstRenderer.TotalUploadAttempts() == beforeUploads &&
                secondRenderer.TotalUploadAttempts() > otherUploads,
            "second mesh upload cannot change first residency totals");
      const auto other = second->TransferMetrics();
      CHECK(other.GeometryDigest == 0 && other.DigestMs == 0,
            "disabled digest is explicit in the second scene");
      CHECK(first->TransferMetrics() == before,
            "second mesh upload cannot overwrite first scene metrics");
      CHECK(first->Carries(3, error), "first scene uploads changed instances");
      CHECK(first->TransferMetrics().GeometryDigest != 0 && second->TransferMetrics() == other,
            "mesh upload remains isolated from the second scene");
      first->Digests(false);
      CHECK(first->Carries(4, error) && first->TransferMetrics().GeometryDigest == 0 &&
                first->TransferMetrics().DigestMs == 0,
            "disabling mesh digest clears only its own measurements");
      CHECK(second->TransferMetrics() == other,
            "clearing one scene leaves the other snapshot intact");
      firstRenderer.WaitForGpu();
      secondRenderer.WaitForGpu();
    }
  }
  SDL_Quit();
  return Report();
}
