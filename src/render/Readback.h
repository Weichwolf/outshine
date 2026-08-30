#ifndef OUTSHINE_RENDER_READBACK_H
#define OUTSHINE_RENDER_READBACK_H

#include <cstdint>

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

enum class ReadState { Ready, Failed };

class Readback {
public:
  ~Readback() { Release(); }

  Readback() = default;
  Readback(const Readback &) = delete;
  Readback &operator=(const Readback &) = delete;

  [[nodiscard]] ReadState FromTexture(SDL_GPUDevice *device,
                                      SDL_GPUTexture *texture,
                                      uint32_t width,
                                      uint32_t height,
                                      uint32_t texelBytes);
  [[nodiscard]] ReadState FromBuffer(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes);

  [[nodiscard]] const uint8_t *Rows() const { return Mapped; }

  [[nodiscard]] uint32_t RowBytes() const { return Row; }

  void Release();

private:
  [[nodiscard]] ReadState Land(SDL_GPUCommandBuffer *commands);

  SDL_GPUDevice *Device = nullptr;
  SDL_GPUTransferBuffer *Transfer = nullptr;
  const uint8_t *Mapped = nullptr;
  uint32_t Row = 0;
};

} // namespace outshine::Render
#endif
