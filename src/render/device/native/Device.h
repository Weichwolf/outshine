#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_DEVICE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_DEVICE_H

#include "Commands.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

struct SDL_Window;

namespace outshine::Render::Native {

struct Capabilities {
  std::string Name;
  uint32_t MaxColourAttachments = 0;
  uint32_t MaxTextureExtent = 0;
  uint32_t MaxTextureLayers = 0;
  uint64_t StorageBufferAlignment = 0;
  uint64_t UniformBufferAlignment = 0;
  uint64_t MaxStorageBufferBytes = 0;
  uint64_t MaxUniformBufferBytes = 0;
  std::array<uint32_t, 3> MaxWorkgroupSize{};
  std::array<uint32_t, 3> MaxWorkgroupCount{};
  uint32_t MaxWorkgroupInvocations = 0;
  bool GpuTimestamps = false;
};

struct Completion {
  uint64_t Device = 0;
  uint64_t Serial = 0;
};

enum class CompletionStatus { Pending, Ready };

struct DeviceOptions {
  bool Validation = false;
  SDL_Window *Window = nullptr;
};

struct ComputeOptions {
  std::string_view Entry = "main";
  std::string_view Label;
};

class Device {
public:
  virtual ~Device() = default;
  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;

  [[nodiscard]] virtual const Capabilities &Limits() const noexcept = 0;
  [[nodiscard]] virtual bool
  Supports(Format format, UseMask<TextureUse> use, bool filtered) const = 0;

  [[nodiscard]] virtual Result<std::unique_ptr<Buffer>>
  CreateBuffer(BufferDescription description, std::string_view label = {}) = 0;
  [[nodiscard]] virtual Result<std::unique_ptr<Texture>>
  CreateTexture(TextureDescription description, std::string_view label = {}) = 0;
  [[nodiscard]] virtual Result<std::unique_ptr<Sampler>>
  CreateSampler(SamplerDescription description, std::string_view label = {}) = 0;
  [[nodiscard]] virtual Result<std::unique_ptr<ComputePipeline>>
  CreateComputePipeline(std::span<const uint32_t> spirv, ComputeOptions options = {}) = 0;

  [[nodiscard]] virtual Result<std::unique_ptr<GraphicsPipeline>>
  CreateGraphicsPipeline([[maybe_unused]] const GraphicsDescription &description) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native graphics pipelines unavailable"});
  }

  [[nodiscard]] virtual Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) = 0;
  [[nodiscard]] virtual Result<std::span<const std::byte>> MapRead(Buffer &buffer,
                                                                   ByteRange range) = 0;
  [[nodiscard]] virtual Result<void> Unmap(Buffer &buffer) = 0;

  [[nodiscard]] virtual Result<std::unique_ptr<Commands>> Begin() = 0;

  [[nodiscard]] virtual Result<void> SetPresentTarget([[maybe_unused]] SDL_Window *window) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native target switching unavailable"});
  }

  [[nodiscard]] virtual Result<std::unique_ptr<PresentFrame>> AcquirePresentFrame() {
    return std::unexpected(
        Error{.Kind = ErrorKind::Unsupported, .Message = "native presentation unavailable"});
  }

  [[nodiscard]] virtual Result<Completion> Submit(std::unique_ptr<Commands> commands) = 0;
  [[nodiscard]] virtual Result<CompletionStatus> Poll(Completion completion) = 0;
  [[nodiscard]] virtual Result<CompletionStatus> Wait(Completion completion,
                                                      std::chrono::nanoseconds budget) = 0;

protected:
  Device() = default;
};

[[nodiscard]] Result<std::unique_ptr<Device>> CreateDevice(DeviceOptions options = {});

}
#endif
