/* A GPU->CPU TRANSFER, AND IT WAITS. SDL_GPU hands back a fence for a submitted command buffer, so
 * the honest shape is "copy, wait, read": the poll this used to be existed because a browser frame
 * thread had no legal way to stand still, and there is no browser.
 *
 * The transfer buffer belongs to the transfer and not to the caller, because the mapped range is
 * only valid between the map and the unmap, and that window has to outlive the call that started
 * it. */
#ifndef READBACK_H
#define READBACK_H

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

  /* Copies the whole texture and waits for the copy to retire. `texelBytes` is what one texel of the
   * texture's own format occupies, which is what decides the row pitch of the answer. */
  [[nodiscard]] ReadState FromTexture(SDL_GPUDevice *device, SDL_GPUTexture *texture, uint32_t width,
                                      uint32_t height, uint32_t texelBytes);
  [[nodiscard]] ReadState FromBuffer(SDL_GPUDevice *device, SDL_GPUBuffer *source, uint32_t bytes);

  /* Valid only after a Ready answer and only until Release(). Rows are tightly packed at
   * `RowBytes()`: SDL_GPU lays a downloaded texture out at the pitch the request named. */
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
