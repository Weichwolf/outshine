/* AN SDL_GPU HANDLE THAT FREES ITSELF. SDL_GPU's objects are raw C handles with a release function
 * each, so a member holding one is an owning raw pointer -- the exact shape `R.1`, `R.3` and `C.31`
 * name. This makes the leak unspellable rather than forbidden: there is no way to hold a handle that
 * is not released, because holding one IS this type.
 *
 * MOVE-ONLY, because a handle has one owner: a copy would release twice. */
#ifndef GPUOWNED_H
#define GPUOWNED_H

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

/* THE DEVICE AND THE SUBSYSTEM IT NEEDS, AS ONE LIFETIME, and it is not the template above because
 * SDL spells this pair differently: a device is destroyed rather than released, and the video
 * subsystem it was opened under is reference-counted beside it. DECLARE IT FIRST in whatever holds
 * it (`C.13`): members are destroyed in reverse declaration order, so a device declared first
 * outlives every handle taken from it. */
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
    if (Device_) {
      SDL_DestroyGPUDevice(Device_);
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    Device_ = nullptr;
  }

private:
  SDL_GPUDevice *Device_ = nullptr;
};

using OwnedBuffer = Owned<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>;
using OwnedTexture = Owned<SDL_GPUTexture, SDL_ReleaseGPUTexture>;
using OwnedSampler = Owned<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
using OwnedShader = Owned<SDL_GPUShader, SDL_ReleaseGPUShader>;
using OwnedPipeline = Owned<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;

} // namespace outshine::Render
#endif
