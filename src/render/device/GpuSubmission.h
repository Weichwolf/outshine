#ifndef OUTSHINE_RENDER_DEVICE_GPUSUBMISSION_H
#define OUTSHINE_RENDER_DEVICE_GPUSUBMISSION_H

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

struct GpuSubmission {
  void *Context = nullptr;
  SDL_GPUCommandBuffer *(*Acquire)(void *, SDL_GPUDevice *) = [](void *, SDL_GPUDevice *device) {
    return SDL_AcquireGPUCommandBuffer(device);
  };
  SDL_GPUFence *(*Submit)(void *, SDL_GPUCommandBuffer *) = [](void *,
                                                               SDL_GPUCommandBuffer *commands) {
    return SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  };
  void *(*MapUpload)(void *, SDL_GPUDevice *, SDL_GPUTransferBuffer *) =
      [](void *, SDL_GPUDevice *device, SDL_GPUTransferBuffer *transfer) {
        return SDL_MapGPUTransferBuffer(device, transfer, false);
      };
  bool (*WaitIdle)(void *, SDL_GPUDevice *) = [](void *, SDL_GPUDevice *device) {
    return SDL_WaitForGPUIdle(device);
  };
};

}
#endif
