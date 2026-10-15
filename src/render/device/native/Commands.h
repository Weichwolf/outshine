#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_COMMANDS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_COMMANDS_H

#include "Compute.h"
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Render::Native {

enum class ErrorKind { Invalid, Unsupported, Busy, OutOfMemory, DeviceLost, Driver };

struct Error {
  ErrorKind Kind = ErrorKind::Driver;
  std::string Message;
};

template <typename Value> using Result = std::expected<Value, Error>;

struct BufferCopy {
  uint64_t SourceOffset = 0;
  uint64_t DestinationOffset = 0;
  uint64_t Bytes = 0;
};

struct TextureRegion {
  uint32_t Mip = 0;
  uint32_t Layer = 0;
  uint32_t X = 0;
  uint32_t Y = 0;
  uint32_t Width = 0;
  uint32_t Height = 0;
};

struct TextureTransfer {
  uint64_t BufferOffset = 0;
  uint64_t RowBytes = 0;
  TextureRegion Region;
};

class Commands {
public:
  virtual ~Commands() = default;
  Commands(const Commands &) = delete;
  Commands &operator=(const Commands &) = delete;

  [[nodiscard]] virtual Result<void>
  Copy(const Buffer &source, Buffer &destination, BufferCopy region) = 0;

  [[nodiscard]] virtual Result<void>
  Copy(const Buffer &source, Texture &destination, TextureTransfer region) = 0;

  [[nodiscard]] virtual Result<void>
  Copy(const Texture &source, Buffer &destination, TextureTransfer region) = 0;

  [[nodiscard]] virtual Result<void> Copy(const Texture &source,
                                          TextureRegion sourceRegion,
                                          Texture &destination,
                                          TextureRegion destinationRegion) = 0;

  [[nodiscard]] virtual Result<void> Dispatch(const ComputePipeline &pipeline,
                                              std::span<const Binding> bindings,
                                              Workgroups groups) = 0;

  virtual void BeginLabel(std::string_view name) = 0;
  virtual void EndLabel() = 0;

protected:
  Commands() = default;
};

}
#endif
