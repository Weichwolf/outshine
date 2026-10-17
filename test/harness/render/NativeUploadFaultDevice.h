#ifndef OUTSHINE_TEST_RENDER_NATIVEUPLOADFAULTDEVICE_H
#define OUTSHINE_TEST_RENDER_NATIVEUPLOADFAULTDEVICE_H

#include "NativeCommandsProxy.h"
#include "NativeDeviceProxy.h"
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Test::Gpu {
enum class UploadFailurePoint {
  Map,
  Unmap,
  Acquire,
  Copy,
  Submit,
  Texture,
  UploadBuffer,
  DeviceBuffer,
  Pipeline
};

struct UploadFailure {
  std::string Message = "injected native submission failure";
  UploadFailurePoint Point = UploadFailurePoint::Submit;
  bool Armed = false;
  unsigned Skip = 0;
  unsigned Rejected = 0;
  unsigned DeviceBufferRequests = 0;
  unsigned VertexBufferAllocations = 0;
  unsigned Waits = 0;
  unsigned ComputePipelineRequests = 0;
  Render::Native::ErrorKind Kind = Render::Native::ErrorKind::Driver;

  [[nodiscard]] bool Reject(UploadFailurePoint point) {
    if (!Armed || Point != point) { return false; }
    if (Skip > 0) {
      --Skip;
      return false;
    }
    Armed = false;
    ++Rejected;
    return true;
  }

  [[nodiscard]] Render::Native::Error Error() const { return {.Kind = Kind, .Message = Message}; }
};

class NativeUploadFaultCommands final : public NativeCommandsProxy {
public:
  NativeUploadFaultCommands(std::unique_ptr<Render::Native::Commands> commands,
                            UploadFailure &failure)
      : NativeCommandsProxy(std::move(commands)), Failure_(failure) {}

  using NativeCommandsProxy::Copy;

  [[nodiscard]] Render::Native::Result<void> Copy(const Render::Native::Buffer &source,
                                                  Render::Native::Buffer &destination,
                                                  Render::Native::BufferCopy region) override {
    if (Failure_.Reject(UploadFailurePoint::Copy)) { return std::unexpected(Failure_.Error()); }
    return NativeCommandsProxy::Copy(source, destination, region);
  }

  [[nodiscard]] Render::Native::Result<void> Copy(const Render::Native::Buffer &source,
                                                  Render::Native::Texture &destination,
                                                  Render::Native::TextureTransfer region) override {
    if (Failure_.Reject(UploadFailurePoint::Copy)) { return std::unexpected(Failure_.Error()); }
    return NativeCommandsProxy::Copy(source, destination, region);
  }

private:
  UploadFailure &Failure_;
};

class NativeUploadFaultDevice final : public NativeDeviceProxy {
public:
  NativeUploadFaultDevice(std::unique_ptr<Render::Native::Device> device, UploadFailure &failure)
      : NativeDeviceProxy(*device), Device_(std::move(device)), Failure_(failure) {}

  [[nodiscard]] Render::Native::Result<Render::Native::CompletionStatus>
  Wait(Render::Native::Completion completion, std::chrono::nanoseconds budget) override {
    ++Failure_.Waits;
    return NativeDeviceProxy::Wait(completion, budget);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::ComputePipeline>>
  CreateComputePipeline(std::span<const uint32_t> spirv,
                        Render::Native::ComputeOptions options = {}) override {
    ++Failure_.ComputePipelineRequests;
    return NativeDeviceProxy::CreateComputePipeline(spirv, options);
  }

  [[nodiscard]] static Render::Native::Result<std::unique_ptr<Render::Native::Device>>
  Create(void *context, Render::Native::DeviceOptions options) {
    auto device = Render::Native::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<NativeUploadFaultDevice>(std::move(*device),
                                                     *static_cast<UploadFailure *>(context));
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Buffer>>
  CreateBuffer(Render::Native::BufferDescription description, std::string_view label) override {
    if (description.Placement == Render::Native::Memory::Device) {
      ++Failure_.DeviceBufferRequests;
    }
    if ((description.Placement == Render::Native::Memory::Device &&
         Failure_.Reject(UploadFailurePoint::DeviceBuffer)) ||
        (description.Placement == Render::Native::Memory::Upload &&
         Failure_.Reject(UploadFailurePoint::UploadBuffer))) {
      return std::unexpected(Failure_.Error());
    }
    auto buffer = Backing().CreateBuffer(description, label);
    if (buffer && (static_cast<unsigned>(description.Use) &
                   static_cast<unsigned>(Render::Native::BufferUse::Vertex)) != 0) {
      ++Failure_.VertexBufferAllocations;
    }
    return buffer;
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Texture>>
  CreateTexture(Render::Native::TextureDescription description, std::string_view label) override {
    if (Failure_.Reject(UploadFailurePoint::Texture)) { return std::unexpected(Failure_.Error()); }
    return Backing().CreateTexture(description, label);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::GraphicsPipeline>>
  CreateGraphicsPipeline(const Render::Native::GraphicsDescription &description) override {
    if (Failure_.Reject(UploadFailurePoint::Pipeline)) { return std::unexpected(Failure_.Error()); }
    return Backing().CreateGraphicsPipeline(description);
  }

  [[nodiscard]] Render::Native::Result<std::span<std::byte>>
  MapWrite(Render::Native::Buffer &buffer, Render::Native::ByteRange range) override {
    if (Failure_.Reject(UploadFailurePoint::Map)) { return std::unexpected(Failure_.Error()); }
    return Backing().MapWrite(buffer, range);
  }

  [[nodiscard]] Render::Native::Result<void> Unmap(Render::Native::Buffer &buffer) override {
    if (Failure_.Reject(UploadFailurePoint::Unmap)) { return std::unexpected(Failure_.Error()); }
    return Backing().Unmap(buffer);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Commands>> Begin() override {
    if (Failure_.Reject(UploadFailurePoint::Acquire)) { return std::unexpected(Failure_.Error()); }
    auto commands = Backing().Begin();
    if (!commands) { return std::unexpected(commands.error()); }
    return std::make_unique<NativeUploadFaultCommands>(std::move(*commands), Failure_);
  }

  [[nodiscard]] Render::Native::Result<Render::Native::Completion>
  Submit(std::unique_ptr<Render::Native::Commands> commands) override {
    if (Failure_.Reject(UploadFailurePoint::Submit)) { return std::unexpected(Failure_.Error()); }
    if (auto *wrapped = dynamic_cast<NativeUploadFaultCommands *>(commands.get());
        wrapped != nullptr) {
      commands = wrapped->Take();
    }
    return Backing().Submit(std::move(commands));
  }

private:
  std::unique_ptr<Render::Native::Device> Device_;
  UploadFailure &Failure_;
};
}
#endif
