#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <dlfcn.h>
#include <string>
#include <vector>
#include "RuntimeScene.h"
#include "FrameCapture.h"
#include "SceneRenderer.h"
#include <memory>
#include "Readback.h"
#include "Check.h"

namespace {
enum class Failure { None, Map, Acquire, Pass, Submit, Allocate, Transfer, Buffer };
Failure nextFailure = Failure::None;
unsigned skipFailures = 0;
unsigned failures = 0;
unsigned quadBufferAllocations = 0;

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

extern "C" SDL_GPUBuffer *SDLCALL SDL_CreateGPUBuffer(SDL_GPUDevice *device,
                                                      const SDL_GPUBufferCreateInfo *info) {
  if (Reject(Failure::Buffer)) { return nullptr; }
  if (info->usage == SDL_GPU_BUFFERUSAGE_VERTEX) { ++quadBufferAllocations; }
  static const auto original = Original<decltype(&SDL_CreateGPUBuffer)>("SDL_CreateGPUBuffer");
  return original(device, info);
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
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "empty scene opens");
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
      CHECK(renderer.ReplaceOverlay(std::span<const OverlayQuad>(&quad, 1), nullptr, error),
            "combined overlay replacement provisions its own default atlas");
      const auto read = [&] {
        std::vector<uint8_t> pixels;
        CHECK(scene->Draw(error), error.c_str());
        CHECK(Core::ReadFrame(renderer, pixels, error), error.c_str());
        return pixels;
      };
      const auto baseline = read();
      CHECK(baseline.size() == 32 * 32 * 4 && baseline[0] == 255,
            "the default atlas produces white overlay pixels");
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
      const unsigned allocated = quadBufferAllocations;
      std::array<OverlayQuad, 2> replacement{quad, quad};
      replacement[0].Red = replacement[0].Green = replacement[0].Blue = 0;
      replacement[1] = replacement[0];
      for (const auto failure : {Failure::Buffer,
                                 Failure::Transfer,
                                 Failure::Map,
                                 Failure::Acquire,
                                 Failure::Pass,
                                 Failure::Submit}) {
        nextFailure = failure;
        const unsigned before = failures;
        error.clear();
        CHECK(!renderer.SetOverlay(replacement.data(), replacement.size(), error) && !error.empty(),
              "quad upload failure is reported");
        CHECK(failures == before + 1 && nextFailure == Failure::None, "quad failure point reached");
        CHECK(read() == baseline, "quad failure preserves previous geometry and count");
        CHECK(renderer.SetOverlay(replacement.data(), replacement.size(), error),
              "quad retry succeeds");
        CHECK(read() != baseline, "quad retry changes the image");
        CHECK(renderer.SetOverlay(&quad, 1, error), "original quad restored");
      }
      CHECK(quadBufferAllocations == allocated + 1,
            "replacement reuses two geometry buffers after warming");
      CHECK(!renderer.SetOverlay(nullptr, 1, error), "nonempty null quad input rejected");
      CHECK(read() == baseline, "null input preserves overlay");
      CHECK(!renderer.SetOverlay(&quad, kMaxOverlayQuads + 1, error),
            "oversized input rejected before reading");
      CHECK(read() == baseline, "oversized input preserves overlay");
      CHECK(renderer.SetOverlay(nullptr, 0, error), "empty input removes overlay");
      CHECK(read() != baseline, "empty overlay no longer draws white");
      CHECK(renderer.SetOverlay(&quad, 1, error), "overlay restored after removal");
      const OverlayDraw::AtlasPixels blackAtlas{.Rgba = black.data(), .Width = 1, .Height = 1};
      const OverlayDraw::AtlasPixels whiteAtlas{.Rgba = white.data(), .Width = 1, .Height = 1};
      for (const auto failure :
           {Failure::Transfer, Failure::Map, Failure::Acquire, Failure::Pass, Failure::Submit}) {
        nextFailure = failure;
        skipFailures = 1;
        const unsigned before = failures;
        CHECK(!renderer.ReplaceOverlay(replacement, &blackAtlas, error),
              "second upload failure rejects combined replacement");
        CHECK(failures == before + 1 && nextFailure == Failure::None,
              "failure reached quad phase after successful atlas upload");
        CHECK(read() == baseline, "combined failure preserves both previous atlas and geometry");
        CHECK(renderer.ReplaceOverlay(replacement, &blackAtlas, error), "combined retry succeeds");
        CHECK(read() != baseline, "combined retry publishes new pixels");
        CHECK(renderer.ReplaceOverlay(std::span<const OverlayQuad>(&quad, 1), &whiteAtlas, error),
              "combined baseline restored");
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
