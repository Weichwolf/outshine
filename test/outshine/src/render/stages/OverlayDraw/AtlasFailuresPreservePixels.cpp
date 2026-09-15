#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <dlfcn.h>
#include <string>
#include <vector>
#include "Live.h"
#include "SceneRenderer.h"
#include <memory>
#include "Readback.h"
#include "Check.h"

namespace {
enum class Failure { None, Map, Acquire, Pass, Submit, Allocate, Transfer };
Failure nextFailure = Failure::None;
unsigned skipFailures = 0;
unsigned failures = 0;

bool Reject(Failure point) {
  if (nextFailure != point) { return false; }
  if (skipFailures > 0) {
    --skipFailures;
    return false;
  }
  nextFailure = Failure::None;
  ++failures;
  SDL_SetError("injected overlay atlas failure");
  return true;
}

template <typename F> F Original(const char *name) {
  auto function = reinterpret_cast<F>(dlsym(RTLD_NEXT, name));
  assert(function != nullptr);
  return function;
}
}

extern "C" SDL_GPUTexture *SDLCALL SDL_CreateGPUTexture(SDL_GPUDevice *device,
                                                        const SDL_GPUTextureCreateInfo *info) {
  if (Reject(Failure::Allocate)) { return nullptr; }
  static const auto original = Original<decltype(&SDL_CreateGPUTexture)>("SDL_CreateGPUTexture");
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
  if (nextFailure == Failure::Submit && skipFailures == 0) {
    const bool consumed = SDL_CancelGPUCommandBuffer(commands);
    assert(consumed);
    (void)Reject(Failure::Submit);
    return false;
  }
  (void)Reject(Failure::Submit);
  static const auto original =
      Original<decltype(&SDL_SubmitGPUCommandBuffer)>("SDL_SubmitGPUCommandBuffer");
  return original(commands);
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  {
    SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "empty scene opens");
    if (scene) {
      Viewpoint eye;
      eye.EyeM = {{0, 0, 5}};
      eye.YfovRad = 1;
      eye.ZNearM = 0.1;
      eye.ZFarM = 100;
      scene->Eye(eye);
      OverlayQuad quad;
      quad.WidthPx = quad.HeightPx = quad.ClipWidthPx = quad.ClipHeightPx = 32;
      quad.U1 = quad.V1 = 1;
      constexpr std::array<uint8_t, 4> white{255, 255, 255, 255};
      constexpr std::array<uint8_t, 4> black{0, 0, 0, 255};
      CHECK(renderer.SetOverlay(&quad, 1, error), "full-frame overlay uploaded");
      const auto read = [&] {
        std::vector<uint8_t> pixels;
        CHECK(scene->Draw(error), error.c_str());
        CHECK(scene->ReadPixels(pixels, error), error.c_str());
        return pixels;
      };
      CHECK(renderer.SetOverlayAtlas(white.data(), 1, 1, error), "white atlas installed");
      const auto baseline = read();
      CHECK(baseline.size() == 32 * 32 * 4 && baseline[0] == 255, "overlay produces white pixels");
      for (const auto failure : {Failure::Allocate,
                                 Failure::Transfer,
                                 Failure::Map,
                                 Failure::Acquire,
                                 Failure::Pass,
                                 Failure::Submit}) {
        nextFailure = failure;
        const unsigned before = failures;
        error.clear();
        CHECK(!renderer.SetOverlayAtlas(black.data(), 1, 1, error) && !error.empty(),
              "upload failure is reported");
        CHECK(failures == before + 1 && nextFailure == Failure::None,
              "requested failure point was reached");
        CHECK(read() == baseline, "rejected upload preserves resident atlas pixels");
        CHECK(renderer.SetOverlayAtlas(black.data(), 1, 1, error), "retry succeeds");
        CHECK(read() != baseline, "retry publishes a genuinely different atlas");
        CHECK(renderer.SetOverlayAtlas(white.data(), 1, 1, error), "baseline restored");
      }
      for (const auto dimensions :
           {std::array{0, 1},
            std::array{1, -1},
            std::array{65536, 65536},
            std::array{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}}) {
        CHECK(!renderer.SetOverlayAtlas(white.data(), dimensions[0], dimensions[1], error),
              "invalid or overflowing dimensions rejected before reading texels");
        CHECK(read() == baseline, "invalid dimensions preserve old pixels");
      }
      CHECK(!renderer.SetOverlayAtlas(nullptr, 1, 1, error), "null texels rejected");
      CHECK(read() == baseline, "null texels preserve old pixels");
    }
  }
  SDL_Quit();
  return Report();
}
