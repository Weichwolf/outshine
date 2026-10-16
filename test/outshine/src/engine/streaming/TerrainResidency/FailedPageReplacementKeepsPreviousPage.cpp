#include "RuntimeScene.h"
#include "HeightSheets.h"
#include "Check.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
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
    outshine::Test::Gpu::UploadFailure failure{.Message = "injected height-page upload failure"};
    Render::SceneRenderer renderer(
        {.Context = &failure, .Create = outshine::Test::Gpu::NativeUploadFaultDevice::Create});
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      const auto uploadBudget = renderer.OfflineUploads(std::chrono::seconds(5));
      HeightSheets sheets;
      sheets.Into(&renderer);
      sheets.Framed(TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0}));
      Patchwork patch;
      patch.Sheets.push_back({.Tile = {.Zoom = 14, .X = 8192, .Y = 8192},
                              .Nodes = std::vector<float>(Render::GroundLattice::kPageNodes, 3),
                              .Side = Render::GroundLattice::kSide,
                              .Postings = Render::GroundLattice::kSide});
      CHECK(sheets.Hands(patch, error), "initial sheet publishes");
      const auto bytes = renderer.HeightPageSourceBytes();
      const auto ownerBytes = sheets.HeapBytes();
      const auto triangles = renderer.GroundLatticeTriangles();
      CHECK(bytes > 0 && triangles > 0, "fixture owns actual height data and topology");
      CHECK(ownerBytes >= Render::GroundLattice::kPageNodes * sizeof(float),
            "height owner counts its retained node capacity");
      Patchwork repeated = patch;
      repeated.Sheets.push_back(patch.Sheets.front());
      CHECK(!sheets.Hands(repeated, error),
            "duplicate tile identity rejects before it can alter resident terrain");
      CHECK(renderer.HeightPageSourceBytes() == bytes &&
                renderer.GroundLatticeTriangles() == triangles,
            "duplicate validation preserves the complete published terrain");
      patch.Sheets.front().Nodes.assign(Render::GroundLattice::kPageNodes, 7);
      failure.Armed = true;
      CHECK(!sheets.Hands(patch, error) && !failure.Armed && failure.Rejected == 1,
            "replacement fails at actual GPU submission");
      CHECK(error.contains("injected height-page upload failure"),
            "replacement reports the native upload failure cause");
      CHECK(renderer.HeightPageSourceBytes() == bytes &&
                renderer.GroundLatticeTriangles() == triangles,
            "failed page upload retains the previous resident page and CPU payload");
      patch.Sheets.front().Nodes.assign(Render::GroundLattice::kPageNodes, 3);
      CHECK(sheets.Hands(patch, error) && renderer.HeightPageSlots() == 1,
            "previous heights remain usable without allocating a replacement page");
      patch.Sheets.front().Nodes.assign(Render::GroundLattice::kPageNodes, 7);
      CHECK(sheets.Hands(patch, error) && renderer.HeightPageSourceBytes() == bytes,
            "valid retry replaces the page and releases its predecessor");
      for (int step = 0; step < 4; ++step) {
        patch.Sheets.front().Nodes.assign(Render::GroundLattice::kPageNodes,
                                          static_cast<float>(10 + step));
        CHECK(sheets.Hands(patch, error) && renderer.HeightPageSlots() == 2 &&
                  renderer.HeightPageSourceBytes() == bytes,
              "replacement overlap stays at two reusable slots with one retained payload");
      }
      sheets.Clear();
      CHECK(renderer.HeightPageSourceBytes() == 0 && renderer.GroundLatticeTriangles() == 0,
            "streaming owner releases the successfully replaced page");
      CHECK(sheets.HeapBytes() > 0 && sheets.HeapBytes() < ownerBytes,
            "cleared height owner releases node payload and reports reusable index capacity");
    }
  }
  SDL_Quit();
  return Report();
}
