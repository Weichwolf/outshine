#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_COMMANDS_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_COMMANDS_H

#include "Compute.h"
#include "Presentation.h"
#include "Render.h"
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Render::Native {

enum class ErrorKind { Invalid, Unsupported, Busy, OutOfMemory, DeviceLost, Driver, Cancelled };

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

  void MarkFrame() noexcept { Frame_ = true; }

  [[nodiscard]] bool IsFrame() const noexcept { return Frame_; }

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

  [[nodiscard]] virtual Result<void>
  DispatchIndirect([[maybe_unused]] const ComputePipeline &pipeline,
                   [[maybe_unused]] std::span<const Binding> bindings,
                   [[maybe_unused]] const Buffer &arguments,
                   [[maybe_unused]] uint64_t offset) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native indirect compute unavailable"});
  }

  [[nodiscard]] virtual Result<void>
  BeginRenderPass([[maybe_unused]] const RenderPassDescription &description) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native raster commands unavailable"});
  }

  [[nodiscard]] virtual Result<void> Draw([[maybe_unused]] const DrawDescription &description) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native raster commands unavailable"});
  }

  [[nodiscard]] virtual Result<void>
  DrawIndexed([[maybe_unused]] const IndexedDrawDescription &description) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native raster commands unavailable"});
  }

  [[nodiscard]] virtual Result<void>
  DrawIndexedIndirect([[maybe_unused]] const IndirectDrawDescription &description) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native raster commands unavailable"});
  }

  [[nodiscard]] virtual Result<void>
  DrawIndexedSequence(const Native::IndexedDrawSequence &sequence) {
    if (sequence.SourceRanges.size() != sequence.Arguments.Count) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "indexed draw sequence has mismatched rows"});
    }
    const auto &arguments = sequence.Arguments;
    if (arguments.Count != 0 &&
        arguments.Offset > std::numeric_limits<uint64_t>::max() -
                               uint64_t{arguments.Count - 1u} * arguments.Stride) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "indexed draw sequence offsets overflow"});
    }
    for (size_t at = 0; at < sequence.SourceRanges.size();) {
      if (const auto &source = sequence.SourceRanges[at]) {
        const auto result = DrawIndexed(
            {.State = sequence.State, .Indices = sequence.SourceIndices, .Range = *source});
        if (!result) { return result; }
        ++at;
        continue;
      }
      size_t end = at + 1;
      while (end < sequence.SourceRanges.size() && !sequence.SourceRanges[end]) { ++end; }
      const auto result =
          DrawIndexedIndirect({.State = sequence.State,
                               .Indices = sequence.CompactedIndices,
                               .Range = {.Data = arguments.Data,
                                         .Offset = arguments.Offset + at * arguments.Stride,
                                         .Count = static_cast<uint32_t>(end - at),
                                         .Stride = arguments.Stride}});
      if (!result) { return result; }
      at = end;
    }
    return {};
  }

  [[nodiscard]] virtual Result<void> EndRenderPass() {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native raster commands unavailable"});
  }

  [[nodiscard]] virtual Result<void> Present([[maybe_unused]] std::unique_ptr<PresentFrame> frame) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native presentation unavailable"});
  }

  virtual void BeginLabel(std::string_view name) = 0;
  virtual void EndLabel() = 0;

protected:
  Commands() = default;

private:
  bool Frame_ = false;
};

}
#endif
