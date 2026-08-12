/* A GPU->CPU TRANSFER AS A STATE. On the browser's frame thread the completion of a map is an event
 * and not a return value: there is no legal way to stand still until it arrives, so every reader is
 * a poll — ask this turn, act on the turn the answer stops being Pending.
 *
 * The staging buffer belongs to the transfer and not to the caller, because the mapped range is only
 * valid between the map and the unmap, and that window has to outlive the call that started it. */
#ifndef READBACK_H
#define READBACK_H

#include <cstdint>

#include <webgpu/webgpu_cpp.h>

namespace outshine::Render {

enum class ReadState { Pending, Ready, Failed };

class Readback {
public:
  [[nodiscard]] bool Idle() const { return !Staging; }

  void FromTexture(const wgpu::Device &device, const wgpu::Queue &queue, const wgpu::Texture &tex,
                   wgpu::TextureAspect aspect, uint32_t width, uint32_t height,
                   uint32_t texelBytes);
  void FromBuffer(const wgpu::Device &device, const wgpu::Queue &queue, const wgpu::Buffer &src,
                  uint64_t bytes);

  [[nodiscard]] ReadState Poll(const wgpu::Instance &instance);

  /* Valid only while Poll answers Ready, and only until Release(). Rows leave the device on a
   * 256-byte pitch (the WebGPU rule), so the stride is part of the answer. */
  const uint8_t *Rows() const;
  uint32_t RowBytes() const { return PaddedRow; }
  void Release();

private:
  void Map(uint64_t bytes);

  wgpu::Buffer Staging;
  uint64_t Bytes = 0;
  uint32_t PaddedRow = 0;
  bool Done = false, Ok = false;
};

} // namespace outshine::Render
#endif
