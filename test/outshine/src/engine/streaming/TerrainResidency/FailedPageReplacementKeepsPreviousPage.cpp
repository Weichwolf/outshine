#include "RuntimeScene.h"
#include "HeightSheets.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <dlfcn.h>

namespace {
bool rejectSubmit = false;

bool RejectCommands(SDL_GPUCommandBuffer *commands) {
  if (!rejectSubmit) { return false; }
  rejectSubmit = false;
  const bool cancelled = SDL_CancelGPUCommandBuffer(commands);
  assert(cancelled);
  SDL_SetError("injected height-page upload failure");
  return true;
}
}

extern "C" bool SDLCALL SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  if (RejectCommands(commands)) { return false; }
  static const auto original = reinterpret_cast<decltype(&SDL_SubmitGPUCommandBuffer)>(
      dlsym(RTLD_NEXT, "SDL_SubmitGPUCommandBuffer"));
  assert(original != nullptr);
  return original(commands);
}

extern "C" SDL_GPUFence *SDLCALL
SDL_SubmitGPUCommandBufferAndAcquireFence(SDL_GPUCommandBuffer *commands) {
  if (RejectCommands(commands)) { return nullptr; }
  static const auto original =
      reinterpret_cast<decltype(&SDL_SubmitGPUCommandBufferAndAcquireFence)>(
          dlsym(RTLD_NEXT, "SDL_SubmitGPUCommandBufferAndAcquireFence"));
  assert(original != nullptr);
  return original(commands);
}

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
      rejectSubmit = true;
      CHECK(!sheets.Hands(patch, error) && !rejectSubmit,
            "replacement fails at actual GPU submission");
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
