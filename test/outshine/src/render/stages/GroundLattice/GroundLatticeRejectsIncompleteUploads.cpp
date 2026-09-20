#include <SDL3/SDL.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <dlfcn.h>
#include <memory>
#include <string>
#include <vector>

#include "Check.h"
#include "GroundLattice.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"

namespace {

enum class Failure { None, Buffer, Transfer, Map, Acquire, Pass, Submit };

Failure Next = Failure::None;

bool Reject(Failure point) {
  if (Next != point) { return false; }
  Next = Failure::None;
  SDL_SetError("injected ground upload failure");
  return true;
}

template <typename F> F Original(const char *name) {
  auto function = reinterpret_cast<F>(dlsym(RTLD_NEXT, name));
  assert(function != nullptr);
  return function;
}

}

extern "C" SDL_GPUBuffer *SDLCALL SDL_CreateGPUBuffer(SDL_GPUDevice *device,
                                                      const SDL_GPUBufferCreateInfo *info) {
  if (Reject(Failure::Buffer)) { return nullptr; }
  static const auto original = Original<decltype(&SDL_CreateGPUBuffer)>("SDL_CreateGPUBuffer");
  return original(device, info);
}

extern "C" SDL_GPUTransferBuffer *SDLCALL
SDL_CreateGPUTransferBuffer(SDL_GPUDevice *device, const SDL_GPUTransferBufferCreateInfo *info) {
  if (Reject(Failure::Transfer)) { return nullptr; }
  static const auto original =
      Original<decltype(&SDL_CreateGPUTransferBuffer)>("SDL_CreateGPUTransferBuffer");
  return original(device, info);
}

extern "C" void *SDLCALL SDL_MapGPUTransferBuffer(SDL_GPUDevice *device,
                                                  SDL_GPUTransferBuffer *buffer,
                                                  bool cycle) {
  if (Reject(Failure::Map)) { return nullptr; }
  static const auto original =
      Original<decltype(&SDL_MapGPUTransferBuffer)>("SDL_MapGPUTransferBuffer");
  return original(device, buffer, cycle);
}

extern "C" SDL_GPUCommandBuffer *SDLCALL SDL_AcquireGPUCommandBuffer(SDL_GPUDevice *device) {
  if (Reject(Failure::Acquire)) { return nullptr; }
  static const auto original =
      Original<decltype(&SDL_AcquireGPUCommandBuffer)>("SDL_AcquireGPUCommandBuffer");
  return original(device);
}

extern "C" SDL_GPUCopyPass *SDLCALL SDL_BeginGPUCopyPass(SDL_GPUCommandBuffer *commands) {
  if (Reject(Failure::Pass)) { return nullptr; }
  static const auto original = Original<decltype(&SDL_BeginGPUCopyPass)>("SDL_BeginGPUCopyPass");
  return original(commands);
}

extern "C" bool SDLCALL SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  if (Next == Failure::Submit) {
    CHECK(SDL_CancelGPUCommandBuffer(commands), "injected ground submission consumes commands");
    (void)Reject(Failure::Submit);
    return false;
  }
  static const auto original =
      Original<decltype(&SDL_SubmitGPUCommandBuffer)>("SDL_SubmitGPUCommandBuffer");
  return original(commands);
}

namespace {

using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

GroundTile Tile(float east) {
  GroundTile tile;
  tile.Instance.Row[12] = east;
  tile.Instance.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
  tile.LowM = -1;
  tile.HighM = 1;
  return tile;
}

}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  {
    SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "ground fixture opens");
    if (scene) {
      std::vector<float> nodes(GroundLattice::kPageNodes);
      CHECK(renderer.PlaceHeightPage(nodes, error) == 0, "first height page is placed");
      for (Failure failure :
           {Failure::Transfer, Failure::Map, Failure::Acquire, Failure::Pass, Failure::Submit}) {
        Next = failure;
        error.clear();
        CHECK(renderer.PlaceHeightPage(nodes, error) == kNoPage &&
                  error.find("injected ground upload failure") != std::string::npos,
              "rejected page upload reports its SDL cause");
        CHECK(Next == Failure::None, "the requested page failure is reached");
        CHECK(renderer.PlaceHeightPage(nodes, error) == 1,
              "rejected page upload returns its reservation for retry");
        renderer.ReleaseHeightPage(1);
      }

      const std::array<GroundTile, 1> one{{Tile(0)}};
      std::array<GroundTile, 65> many{};
      for (size_t at = 0; at < many.size(); ++at) { many[at] = Tile(static_cast<float>(at)); }
      CHECK(renderer.SetGroundLattice(one, {}, error), "baseline ground tile is published");
      const uint32_t baseline = renderer.GroundLatticeTriangles();
      for (Failure failure : {Failure::Buffer,
                              Failure::Transfer,
                              Failure::Map,
                              Failure::Acquire,
                              Failure::Pass,
                              Failure::Submit}) {
        Next = failure;
        error.clear();
        CHECK(!renderer.SetGroundLattice(many, {}, error) &&
                  error.find("injected ground upload failure") != std::string::npos,
              "rejected tile upload reports its SDL cause");
        CHECK(Next == Failure::None, "the requested tile failure is reached");
        CHECK(renderer.GroundLatticeTriangles() == baseline,
              "rejected tile upload preserves the published tile count");
      }
      CHECK(renderer.SetGroundLattice(many, {}, error), "tile upload retries successfully");
      CHECK(renderer.GroundLatticeTriangles() == many.size() * GroundLattice::kIndices / 3u,
            "successful tile upload publishes every tile");
    }
  }
  SDL_Quit();
  return Report();
}
