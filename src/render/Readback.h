#ifndef OUTSHINE_RENDER_READBACK_H
#define OUTSHINE_RENDER_READBACK_H

#include "ReadbackTransfer.h"
#include <Extent.h>
#include <cstdint>
#include <cstddef>
#include <span>
#include <optional>
#include <string>

namespace outshine::Render {
enum class ReadState { Ready, Pending, Failed };

class Readback {
public:
  Readback() = default;
  Readback(Readback &&from) noexcept;
  Readback &operator=(Readback &&from) noexcept;
  Readback(const Readback &) = delete;
  Readback &operator=(const Readback &) = delete;
  [[nodiscard]] ReadState
  FromTexture(Native::Device *device, Native::Texture *texture, Extent size, uint32_t texelBytes);
  [[nodiscard]] ReadState
  FromBuffer(Native::Device *device, Native::Buffer *source, uint32_t bytes);
  [[nodiscard]] ReadState Enqueue(Native::Device *device, Native::Buffer *source, uint32_t bytes);
  [[nodiscard]] ReadState Poll();

  [[nodiscard]] const uint8_t *Rows() const {
    return reinterpret_cast<const uint8_t *>(Mapped_.data());
  }

  [[nodiscard]] uint64_t RowBytes() const { return Transfer_ ? Transfer_->RowBytes() : 0; }

  [[nodiscard]] const std::string &WhyNot() const { return Error_; }

  void Release();

private:
  [[nodiscard]] ReadState Begin(Native::Device &device, Native::Result<ReadbackTransfer> transfer);
  [[nodiscard]] ReadState Land();
  [[nodiscard]] ReadState Map(Native::Result<std::optional<std::span<const std::byte>>> result);
  std::optional<ReadbackTransfer> Transfer_;
  Native::Device *Device_ = nullptr;
  std::span<const std::byte> Mapped_;
  std::string Error_;
};
}
#endif
