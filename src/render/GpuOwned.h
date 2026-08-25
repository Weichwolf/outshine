#ifndef OUTSHINE_RENDER_GPUOWNED_H
#define OUTSHINE_RENDER_GPUOWNED_H

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

template <typename Handle, void (*Free)(SDL_GPUDevice *, Handle *)>
class Owned {
public:
  Owned() = default;
  Owned(SDL_GPUDevice *device, Handle *handle) : Device_(device), Handle_(handle) {}
  ~Owned() { Reset(); }
  Owned(Owned &&from) noexcept : Device_(from.Device_), Handle_(from.Handle_) {
    from.Handle_ = nullptr;
  }
  Owned &operator=(Owned &&from) noexcept {
    if (this != &from) {
      Reset();
      Device_ = from.Device_;
      Handle_ = from.Handle_;
      from.Handle_ = nullptr;
    }
    return *this;
  }
  Owned(const Owned &) = delete;
  Owned &operator=(const Owned &) = delete;

  [[nodiscard]] Handle *Get() const { return Handle_; }
  explicit operator bool() const { return Handle_ != nullptr; }
  void Reset() {
    if (Handle_) { Free(Device_, Handle_); }
    Handle_ = nullptr;
  }

private:
  SDL_GPUDevice *Device_ = nullptr;
  Handle *Handle_ = nullptr;
};

class OwnedDevice {
public:
  OwnedDevice() = default;
  explicit OwnedDevice(SDL_GPUDevice *device) : Device_(device) {}
  ~OwnedDevice() { Reset(); }
  OwnedDevice(OwnedDevice &&from) noexcept : Device_(from.Device_) { from.Device_ = nullptr; }
  OwnedDevice &operator=(OwnedDevice &&from) noexcept {
    if (this != &from) {
      Reset();
      Device_ = from.Device_;
      from.Device_ = nullptr;
    }
    return *this;
  }
  OwnedDevice(const OwnedDevice &) = delete;
  OwnedDevice &operator=(const OwnedDevice &) = delete;

  [[nodiscard]] SDL_GPUDevice *Get() const { return Device_; }
  explicit operator bool() const { return Device_ != nullptr; }
  void Reset() {
    if (Device_) { SDL_DestroyGPUDevice(Device_); }
    Device_ = nullptr;
  }

private:
  SDL_GPUDevice *Device_ = nullptr;
};

using OwnedBuffer = Owned<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>;
using OwnedTexture = Owned<SDL_GPUTexture, SDL_ReleaseGPUTexture>;

using OwnedTransfer = Owned<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>;
using OwnedSampler = Owned<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
using OwnedShader = Owned<SDL_GPUShader, SDL_ReleaseGPUShader>;
using OwnedPipeline = Owned<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;
using OwnedComputePipeline = Owned<SDL_GPUComputePipeline, SDL_ReleaseGPUComputePipeline>;

}
#endif
