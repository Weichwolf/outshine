#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <dlfcn.h>

#include "Check.h"
#include "GpuOwned.h"
#include "SubjectResidency.h"

namespace {
bool reject = false;
unsigned rejected = 0;
}

extern "C" bool SDLCALL SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  if (reject) {
    reject = false;
    ++rejected;
    assert(SDL_CancelGPUCommandBuffer(commands));
    SDL_SetError("injected mip submission failure");
    return false;
  }
  static const auto original = reinterpret_cast<decltype(&SDL_SubmitGPUCommandBuffer)>(
      dlsym(RTLD_NEXT, "SDL_SubmitGPUCommandBuffer"));
  assert(original != nullptr);
  return original(commands);
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const OwnedDevice device(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        false,
        nullptr));
    CHECK(device, "device opens");
    if (!device) { return Report(); }
    SubjectResidency residency;
    residency.StandsOn(device.Get());
    const std::array<uint8_t, 16> pixels = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    const SubjectTexture texture{.Rgba = pixels.data(), .Width = 2, .Height = 2};
    reject = true;
    const auto failed =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(!failed && rejected == 1 && !reject,
          "a complete mip submission failure returns no partially usable image");
    const auto retry =
        residency.Upload(texture, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    CHECK(retry && retry->Image && retry->Sample,
          "an immediate mip upload retry returns a complete sampled image");
  }
  SDL_Quit();
  return Report();
}
