#ifndef OUTSHINE_RENDER_DEVICE_GPU_H
#define OUTSHINE_RENDER_DEVICE_GPU_H

#include <SDL3/SDL_gpu.h>

#include "RenderCatalogue.h"

namespace outshine::Render {

struct Gpu {
  SDL_GPUDevice *Device = nullptr;
  SDL_GPUTextureFormat HdrFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
  SDL_GPUTextureFormat SurfaceFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
  int Width = 0, Height = 0;

  bool FiltersFloat32 = false;

  AttachmentSet SceneColours;
};

struct PassRecording {
  SDL_GPUCommandBuffer *Commands = nullptr;
  SDL_GPURenderPass *Pass = nullptr;
  SDL_GPUComputePass *Dispatch = nullptr;
};

} // namespace outshine::Render
#endif
