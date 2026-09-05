#ifndef OUTSHINE_RENDER_READBACK_H
#define OUTSHINE_RENDER_READBACK_H

#include <cstdint>

#include <SDL3/SDL_gpu.h>

#include <Extent.h>

namespace outshine::Render {

enum class ReadState { Ready, Pending, Failed };

class Readback {
public:
  ~Readback() { Release(); }

  Readback() = default;
  Readback(const Readback &) = delete;
  Readback &operator=(const Readback &) = delete;

  [[nodiscard]] ReadState
  FromTexture(SDL_GPUDevice *device, SDL_GPUTexture *texture, Extent size, uint32_t texelBytes);
  [[nodiscard]] ReadState FromBuffer(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes);
  [[nodiscard]] ReadState Enqueue(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes);
  [[nodiscard]] ReadState Poll();

  [[nodiscard]] const uint8_t *Rows() const { return Mapped; }

  [[nodiscard]] uint32_t RowBytes() const { return Row; }

  void Release();

private:
  [[nodiscard]] ReadState Submit(SDL_GPUCommandBuffer *commands);
  [[nodiscard]] ReadState Map();
  [[nodiscard]] ReadState Land(SDL_GPUCommandBuffer *commands);
  [[nodiscard]] ReadState Copies(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes);

  SDL_GPUDevice *Device = nullptr;
  SDL_GPUFence *Fence = nullptr;
  SDL_GPUCommandBuffer *Commands = nullptr;
  SDL_GPUTransferBuffer *Transfer = nullptr;
  const uint8_t *Mapped = nullptr;
  uint32_t Row = 0;
};

} // namespace outshine::Render
#endif
