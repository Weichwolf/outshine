#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "Gpu.h"
#include "SubjectDraw.h"

using outshine::Render::AttachmentSet;
using outshine::Render::Gpu;
using outshine::Render::Resource;
using outshine::Render::SubjectDraw;

int main() {
  using namespace outshine::Test;

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    CHECK(false, "the video subsystem starts, which a shader case needs a device for");
    return Report();
  }
  SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  if (device == nullptr) {
    CHECK(false, "a Metal device is created");
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return Report();
  }

  const Resource optional[3] = {Resource::SceneVelocity, Resource::SceneShadingNormal,
                                Resource::SceneSurfaceIdentity};
  int compiled = 0;
  for (unsigned mask = 0; mask < 8u; ++mask) {
    Gpu gpu;
    gpu.Device = device;
    gpu.FiltersFloat32 = true;

    bool held = gpu.SceneColours.Add(Resource::SceneHdr);
    std::string named = "SceneHdr";
    for (int at = 0; at < 3; ++at) {
      if ((mask & (1u << (unsigned)at)) == 0u) { continue; }
      held = gpu.SceneColours.Add(optional[at]) && held;
      named += at == 0 ? " + velocity" : (at == 1 ? " + shadingNormal" : " + surfaceIdentity");
    }
    CHECK(held, "the attachment set holds every target this combination names");

    SubjectDraw unit;
    std::string error;
    const bool ok = unit.Configure(gpu, error);
    CHECK(ok, "every shader the subject unit emits for this attachment set is accepted by the driver");
    if (!ok) {
      std::printf("       %s: %s\n", named.c_str(), error.c_str());
    } else {
      ++compiled;
    }
  }

  Note("attachment sets compiled", (double)compiled, "of 8");
  Covers("I.26 the subject unit's shader text compiles on a real device, over every attachment set "
         "the compiled plan can hand it");
  SDL_DestroyGPUDevice(device);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return Report();
}
